/*
 * tcp_stream_filter.cc
 *
 *  Created on: Feb 12, 2020
 *      Author: tomer
 */

#include "tcp_stream_buffer_filter.h"
#include "log.h"
#include "async_io.h"
#include "config.h"

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cerrno>
#include <cstring>

namespace ael {

std::ostream& operator<<(std::ostream &out, const TCPStreamBufferFilter *filter) {
	const StreamBufferFilter *stream_buffer_filter = filter;
	out << stream_buffer_filter << " handle=" << filter->handle_;
	return out;
}

TCPStreamBufferFilter::TCPStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, Handle handle, bool pending_connect) :
		StreamBufferFilter(stream_buffer),
		handle_(handle),
		pending_connect_(pending_connect) {}

TCPStreamBufferFilter::~TCPStreamBufferFilter() {}

std::shared_ptr<TCPStreamBufferFilter> TCPStreamBufferFilter::Create(std::shared_ptr<StreamBuffer> stream_buffer, Handle handle, bool connected) {
	LOG_TRACE("creating a tcp stream buffer filter handle=" << handle);
	return std::shared_ptr<TCPStreamBufferFilter>(new TCPStreamBufferFilter(stream_buffer, handle, connected));
}

InResult TCPStreamBufferFilter::In() {
	std::uint8_t buf[100000];

	auto read_ret_ = recv(handle_, buf, sizeof(buf), MSG_DONTWAIT);

	switch (read_ret_) {
	case 0:
		LOG_DEBUG("read EOF " << this);
		return InResult::CreateShouldClose();
	case -1:
		switch (errno) {
		case EAGAIN:
			LOG_DEBUG("read would block " << this);
			return InResult();
		case EFAULT:
		case EINVAL:
		case ENOTCONN:
		case ENOTSOCK:
		case EBADF:
			throw std::system_error(errno, std::system_category(), "read failed");
		default:
			LOG_DEBUG("read EOF with error " << this << " error=" << std::strerror(errno));
			return InResult::CreateShouldClose();
		}
		break;
	default:
		LOG_DEBUG("read " << read_ret_ << " bytes " << this);
		return InResult(buf, read_ret_);
	}
}

OutResult TCPStreamBufferFilter::Out(std::shared_ptr<const DataView> &data_view) {
	auto write_ret = send(handle_, data_view->GetData(), data_view->GetDataLength(), MSG_NOSIGNAL | MSG_DONTWAIT);

	if (write_ret == -1) {
		switch (errno) {
		case EAGAIN:
			LOG_DEBUG("write would block " << this);
			return OutResult();
		case EBADF:
		case EDESTADDRREQ:
		case EFAULT:
		case EINVAL:
		case EMSGSIZE:
		case ENOMEM:
		case ENOTCONN:
		case ENOTSOCK:
		case EOPNOTSUPP:
			throw std::system_error(errno, std::system_category(), "write failed");
		default:
			LOG_DEBUG("tcp stream buffer filter write no longer writable " << this << " error=" << std::strerror(errno));
			return OutResult::CreateShouldClose();
		}
	}

	LOG_DEBUG("write " << write_ret << " bytes " << this);

	if (write_ret == 0) {
		throw "write return 0 (kernel bug?)";
	}

	std::shared_ptr<const DataView> suffix_data_view;
	if (write_ret < data_view->GetDataLength()) {
		data_view = data_view->Slice(write_ret).Save();
		LOG_TRACE("partial write " <<  suffix_data_view->GetDataLength( )<< " bytes left id=" << this);
	} else {
		data_view = nullptr;
	}

	return OutResult();
}

ConnectResult TCPStreamBufferFilter::Accept() {
	LOG_TRACE("accept " << this);

	if (pending_connect_) {
		return ConnectResult::CreateSuccess();
	}

	throw "unexpected use case - connection should already be accepted";
}

ConnectResult TCPStreamBufferFilter::Connect() {
	LOG_TRACE("connect " << this);

	if (pending_connect_) {
		return ConnectResult::CreateSuccess();
	}

	int socket_error;
	socklen_t socket_error_len = sizeof(socket_error);

	if (getsockopt(handle_, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) < 0) {
		throw std::system_error(errno, std::system_category(), "getsockopt failed");
	}

	if (socket_error != 0) {
		LOG_DEBUG("connect - failed on socket error " << this << " socket_error=" << std::strerror(socket_error));
		return ConnectResult::CreateFailed();
	}

	LOG_DEBUG("connect - complete " << this);
	return ConnectResult::CreateSuccess();
}

ShutdownResult TCPStreamBufferFilter::Shutdown() {
	LOG_TRACE("shutdown " << this);
	return ShutdownResult(true);
}

} /* namespace ael */
