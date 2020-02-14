/*
 * tcp_stream_filter.cc
 *
 *  Created on: Feb 12, 2020
 *      Author: tomer
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "tcp_stream_buffer_filter.h"
#include "log.h"
#include "async_io.h"

namespace ael {

std::ostream& operator<<(std::ostream &out, const TCPStreamBufferFilter *filter) {
	const StreamBufferFilter *stream_buffer_filter = filter;
	out << stream_buffer_filter << " fd=" << filter->fd_;
	return out;
}

TCPStreamBufferFilter::TCPStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, int fd, bool pending_connect) :
		StreamBufferFilter(stream_buffer),
		fd_(fd),
		pending_connect_(pending_connect) {}

TCPStreamBufferFilter::~TCPStreamBufferFilter() {}

std::shared_ptr<TCPStreamBufferFilter> TCPStreamBufferFilter::Create(std::shared_ptr<StreamBuffer> stream_buffer, int fd, bool connected) {
	LOG_TRACE("creating a tcp stream buffer filter fd=" << fd);
	return std::shared_ptr<TCPStreamBufferFilter>(new TCPStreamBufferFilter(stream_buffer, fd, connected));

}

InResult TCPStreamBufferFilter::In(std::uint8_t *buf, std::uint32_t buf_size) {
	auto read_ret_ = recv(fd_, buf, buf_size, MSG_DONTWAIT);

	switch (read_ret_) {
	case 0:
		LOG_DEBUG("read EOF " << this);
		return InResult::CreateShouldClose();
	case -1:
		switch (errno) {
		case EAGAIN:
			LOG_DEBUG("read would block " << this);
			return InResult::CreateWouldBlock();
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
		return InResult::CreateHasData(buf, read_ret_);
	}

}

OutResult TCPStreamBufferFilter::Out(std::list<std::shared_ptr<const DataView>> &out_list) {
	while (!out_list.empty()) {
		auto data_view = out_list.front();
		auto has_more = out_list.size() > 1;

		auto write_ret = send(fd_, data_view->GetData(), data_view->GetDataLength(), (has_more ? MSG_MORE : 0) | MSG_NOSIGNAL | MSG_DONTWAIT);
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

		out_list.pop_front();

		std::shared_ptr<const DataView> suffix_data_view;
		if (write_ret < data_view->GetDataLength()) {
			out_list.push_front(data_view->Slice(write_ret).Save());
			LOG_TRACE("partial write " <<  suffix_data_view->GetDataLength( )<< " bytes left id=" << this);
		}
	}

	return OutResult();
}

void TCPStreamBufferFilter::Connect() {
	LOG_TRACE("connect " << this);

	if (pending_connect_) {
		connected_ = true;
		return;
	}

	int socket_error;
	socklen_t socket_error_len = sizeof(socket_error);

	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) < 0) {
		throw std::system_error(errno, std::system_category(), "getsockopt failed");
	}

	if (socket_error != 0) {
		LOG_DEBUG("connect - failed on socket error " << this << " socket_error=" << std::strerror(socket_error));
		read_closed_ = true;
		write_closed_ = true;
		return;
	}

	connected_ = true;
	LOG_DEBUG("connect - complete " << this);
}

} /* namespace ael */
