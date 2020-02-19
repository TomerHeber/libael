/*
 * ssl_stream_buffer_filter.cc
 *
 *  Created on: Feb 13, 2020
 *      Author: tomer
 */

#include "ssl_stream_buffer_filter.h"
#include "async_io.h"
#include "log.h"

#include <cstring>

#include <openssl/err.h>

namespace ael {

SSLStreamBufferFilter::SSLStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, SSL *ssl) :
		StreamBufferFilter(stream_buffer),
		ssl_(ssl),
		rbio_(BIO_new(BIO_s_mem())),
		wbio_(BIO_new(BIO_s_mem())),
		mode_set_(false),
		fatal_error_(false),
		write_not_allowed_(false) {
	SSL_set0_rbio(ssl_, rbio_);
	SSL_set0_wbio(ssl_, wbio_);
	SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
}

SSLStreamBufferFilter::~SSLStreamBufferFilter() {
	SSL_free(ssl_);
}

std::string GetErrors()
{
    BIO *bio = BIO_new(BIO_s_mem());
    ERR_print_errors(bio);
    char *buf;
    size_t len = BIO_get_mem_data(bio, &buf);
    std::string ret(buf, len);
    BIO_free(bio);
    return ret;
}

ConnectResult SSLStreamBufferFilter::Connect() {
	if (!mode_set_) {
		SSL_set_connect_state(ssl_);
		mode_set_ = true;
	}

	return ConnectOrAccept(true);
}

ConnectResult SSLStreamBufferFilter::Accept() {
	if (!mode_set_) {
		SSL_set_accept_state(ssl_);
		mode_set_ = true;
	}

	return ConnectOrAccept(false);
}

ConnectResult SSLStreamBufferFilter::ConnectOrAccept(bool isConnect) {
	LOG_TRACE((isConnect ? "connect " : "accept ") << this)

	while (true) {
		DoBIOOut();

		ERR_clear_error();

		int ret;
		if (isConnect) {
			ret = SSL_connect(ssl_);
		} else {
			ret = SSL_accept(ssl_);
		}

		auto result = HandleErr(SSL_get_error(ssl_, ret));

		if (ret == 0) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " failed " << this << " errors=" << GetErrors());
			return ConnectResult::CreateFailed();
		}

		if (ret == 1) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " success " << this);
			return ConnectResult::CreateSuccess();
		}

		if (result == Failed) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " failed " << this);
			return ConnectResult::CreateFailed();
		}

		if (result == WouldBlock) {
			LOG_TRACE((isConnect ? "connect" : "accept") << " pending " << this);
			return ConnectResult::CreatePending();
		}

		LOG_TRACE((isConnect ? "connect" : "accept") << " keep trying " << this)
	}
}

InResult SSLStreamBufferFilter::In() {
	LOG_TRACE("in " << this)

	while (true) {
		DoBIOOut();

		char buf[16384];

		ERR_clear_error();
		auto ret = SSL_read(ssl_, buf, sizeof(buf));
		auto result = HandleErr(SSL_get_error(ssl_, ret));

		if (ret > 0) {
			LOG_DEBUG("SSL_read received " << ret << " bytes " << this);
			return InResult(reinterpret_cast<const std::uint8_t*>(buf), ret);
		}

		if (result == WouldBlock) {
			LOG_TRACE("read would block " << this);
			return InResult();
		}

		if (result == Failed) {
			LOG_TRACE("read failed " << this);
			return InResult::CreateShouldClose();
		}

		LOG_TRACE("read keep trying " << this);
	}
}

OutResult SSLStreamBufferFilter::Out(std::shared_ptr<const DataView> &data_view) {
	LOG_TRACE("out " << this)

	if (write_not_allowed_) {
		LOG_TRACE("In the process of shutdown SSL_write not allowed " << this);
		return OutResult::CreateShouldClose();
	}

	while (true) {
		DoBIOOut();

		ERR_clear_error();
		auto ret = SSL_write(ssl_, data_view->GetData(), data_view->GetDataLength());
		auto result = HandleErr(SSL_get_error(ssl_, ret));

		if (ret > 0) {
			LOG_DEBUG("SSL_write written " << ret << " bytes " << this);
			if (ret == data_view->GetDataLength()) {
				LOG_TRACE("SSL_write all data written " << this);
				data_view = nullptr;
				return OutResult();
			} else {
				LOG_TRACE("SSL_write there is still data to write " << this);
				data_view = data_view->Slice(ret).Save();
			}
		}

		if (result == WouldBlock) {
			LOG_TRACE("write would block " << this);
			return OutResult();
		}

		if (result == Failed) {
			LOG_TRACE("write failed " << this);
			return OutResult::CreateShouldClose();
		}

		LOG_TRACE("write keep trying " << this);
	}
}

ShutdownResult SSLStreamBufferFilter::Shutdown() {
	auto shutdown_state = SSL_get_shutdown(ssl_);

	LOG_TRACE("shutdown " << this << " state=" << shutdown_state);

	if (fatal_error_) {
		LOG_TRACE("shutdown complete (fatal error) " << this)
		return ShutdownResult(true);
	}

	if (shutdown_state == 0) {
		write_not_allowed_ = true;
	}

	if (!(shutdown_state & SSL_SENT_SHUTDOWN)) {
		LOG_TRACE("sending close_notify " << this)
		while (true) {
			ERR_clear_error();
			auto ret = SSL_shutdown(ssl_);

			if (ret == 0 || ret == 1) {
				LOG_TRACE("sent close_notify " << this)
				break;
			}

			auto result = HandleErr(SSL_get_error(ssl_, ret));

			if (result == WouldBlock) {
				LOG_TRACE("shutdown would block " << this);
				return ShutdownResult(false);
			}

			if (result == Failed) {
				LOG_TRACE("shutdown failed (dirty)" << this);
				return ShutdownResult(true);
			}
		}
	}

	auto out_result = DoBIOOut();

	if (out_result == Failed) {
		LOG_TRACE("shutdown complete (prev out failed) " << this);
		return ShutdownResult(true);
	}

	if (out_result == WouldBlock) {
		LOG_TRACE("shutdown pending (prev out would block) " << this);
		return ShutdownResult(false);
	}

	LOG_TRACE("shutdown complete " << this << " state=" << SSL_get_shutdown(ssl_));

	return ShutdownResult(true);
}

SSLStreamBufferFilter::BIOResult SSLStreamBufferFilter::HandleErr(int err) {
	LOG_TRACE("handle error " << this << " err=" << err)

	if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL) {
		fatal_error_ = true;
		LOG_DEBUG("unrecoverable (fatal) SSL error " << this << " errors=" << GetErrors());
		return Failed;
	}

	if (err == SSL_ERROR_ZERO_RETURN) {
		LOG_TRACE("zero return error (shutdown received)" << this)
		return Failed;
	}

	auto result_out = DoBIOOut();

	if (err == SSL_ERROR_WANT_READ) {
		auto result_in = DoBIOIn();

		if (result_in == Failed) {
			LOG_TRACE("bio in result failed " << this)
			return Failed;
		} else if (result_in == WouldBlock) {
			LOG_TRACE("bio in result would block " << this)
			return WouldBlock;
		}
	}

	if (SSL_ERROR_WANT_WRITE) {
		if (result_out == WouldBlock) {
			LOG_TRACE("bio out result would block " << this);
			return WouldBlock;
		}

		if (result_out == Failed) {
			LOG_TRACE("bio out result failed " << this);
			return Failed;
		}
	}

	return Success;
}

SSLStreamBufferFilter::BIOResult SSLStreamBufferFilter::DoBIOOut() {
	LOG_TRACE("read wbio and out prev " << this)

	while (true) {
		if (wbio_data_view_ && wbio_data_view_->GetDataLength() > 0) {
			LOG_TRACE("wbio_data_view has " << wbio_data_view_->GetDataLength() << " bytes")

			auto out_result = PrevOut(wbio_data_view_);

			if (out_result.ShouldCloseWrite()) {
				LOG_TRACE("out prev should close write " << this)
				return Failed;
			}

			if (wbio_data_view_) {
				LOG_TRACE("out prev partial write " << this)
				return WouldBlock;
			}

			LOG_TRACE("out prev written all data in wbio_data_view")
		}

		if (BIO_eof(wbio_)) {
			LOG_TRACE("wbio is empty");
			return Success;
		}

		char *data;
		auto data_len = BIO_get_mem_data(wbio_, &data);
		wbio_data_view_ = DataView(reinterpret_cast<const std::uint8_t*>(data), data_len).Save();
		BIO_reset(wbio_);

		LOG_TRACE("wbio has " << data_len << " bytes moving to wbio_data_view")
	}
}

SSLStreamBufferFilter::BIOResult SSLStreamBufferFilter::DoBIOIn() {
	LOG_TRACE("in prev and write rbio " << this)

	auto in_result = PrevIn();

	if (in_result.ShouldCloseRead()) {
		LOG_TRACE("in prev should close read " << this)
		return Failed;
	}

	if (!in_result.HasData()) {
		LOG_TRACE("in prev no data " << this)
		return WouldBlock;
	}

	auto data_view = in_result.GetData();

	if (BIO_write(rbio_, data_view->GetData(), data_view->GetDataLength()) != data_view->GetDataLength()) {
		throw "BIO_write failed (not all data was written - out of memory?)";
	}

	LOG_TRACE("written " << data_view->GetDataLength() << " bytes to rbio")

	return Success;
}

} /* namespace ael */
