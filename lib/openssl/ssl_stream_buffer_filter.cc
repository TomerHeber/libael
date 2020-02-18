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
		mode_set_(false) {
	SSL_set0_rbio(ssl_, rbio_);
	SSL_set0_wbio(ssl_, wbio_);

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
	while (true) {
		ERR_clear_error();

		int ret;
		if (isConnect) {
			ret = SSL_connect(ssl_);
		} else {
			ret = SSL_accept(ssl_);
		}

		if (ret == 0) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " failed " << this << " errors=" << GetErrors());
			return ConnectResult::CreateFailed();
		}

		if (ret == 1) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " success " << this);
			return ConnectResult::CreateSuccess();
		}

		auto result = HandleErr(SSL_get_error(ssl_, ret));

		if (result == Failed) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " failed " << this);
			return ConnectResult::CreateFailed();
		}

		if (result == WouldBlock) {
			LOG_DEBUG((isConnect ? "connect" : "accept") << " pending " << this);
			return ConnectResult::CreatePending();
		}

		LOG_TRACE((isConnect ? "connect" : "accept") << " keep trying " << this)
	}
}

SSLStreamBufferFilter::BIOResult SSLStreamBufferFilter::HandleErr(int err) {
	LOG_TRACE("handle error " << this << " err=" << err)

	if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL) {
		LOG_DEBUG("failed " << this << " errors=" << GetErrors());
		return Failed;
	}

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

	auto result_out = DoBIOOut();

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
	LOG_TRACE("read wbio and out prev" << this)

	BUF_MEM *buf_mem = NULL;
	BIO_get_mem_ptr(wbio_, buf_mem);

	if (buf_mem->length == 0) {
		LOG_TRACE("wbio is empty");
		return Success;
	}

	DataView data_view(reinterpret_cast<const std::uint8_t*>(buf_mem->data), buf_mem->length);

	std::list<std::shared_ptr<const DataView>> out_list;
	out_list.push_back(data_view.Save());

	auto out_result = PrevOut(out_list);

	if (out_result.ShouldCloseWrite()) {
		LOG_TRACE("out prev failed " << this);
		return Failed;
	}

	if (out_list.empty()) {
		LOG_TRACE("out prev all data was written " << this);
		buf_mem->length = 0;
		return Success;
	}

	if (out_list.size() > 1) {
		throw "out list unexpected size";
	}

	auto new_data_view = out_list.front();

	if (new_data_view->GetDataLength() == data_view.GetDataLength()) {
		LOG_TRACE("out prev no data written (no progress) " << this);
		return WouldBlock;
	}

	if (new_data_view->GetDataLength() > data_view.GetDataLength()) {
		throw "new data view unexpected size";
	}

	buf_mem->length = new_data_view->GetDataLength();
	std::memcpy(buf_mem->data, new_data_view->GetData(), new_data_view->GetDataLength());

	LOG_TRACE("out prev some data was written " << this);

	return Success;
}

SSLStreamBufferFilter::BIOResult SSLStreamBufferFilter::DoBIOIn() {
	LOG_TRACE("in prev and write rbio" << this)

	BUF_MEM *buf_mem = NULL;
	BIO_get_mem_ptr(rbio_, buf_mem);

	if (buf_mem->length == buf_mem->max) {
		LOG_ERROR("rbio is full");
		return Success;
	}
}


} /* namespace ael */
