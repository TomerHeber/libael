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

#define NOT_CONNECTED 0
#define CONNECTED_BUT_NOT_NOTIFIED 1
#define CONNECTED 2

namespace ael {

TCPStreamBufferFilter::TCPStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, int fd, int connected) :
		StreamBufferFilter(stream_buffer), fd_(fd), connected_(connected), write_closed_(false), read_closed_(false) {}

TCPStreamBufferFilter::~TCPStreamBufferFilter() {}

std::shared_ptr<TCPStreamBufferFilter> TCPStreamBufferFilter::Create(std::shared_ptr<StreamBuffer> stream_buffer, int fd, bool is_connected) {
	LOG_TRACE("creating a tcp stream buffer filter fd=" << fd);
	if (is_connected) {
		return std::shared_ptr<TCPStreamBufferFilter>(new TCPStreamBufferFilter(stream_buffer, fd, CONNECTED_BUT_NOT_NOTIFIED));
	} else {
		return std::shared_ptr<TCPStreamBufferFilter>(new TCPStreamBufferFilter(stream_buffer, fd, NOT_CONNECTED));
	}
}

void TCPStreamBufferFilter::Write(const std::list<std::shared_ptr<const DataView>> &write_list) {
	if (write_closed_) {
		LOG_TRACE("tcp stream buffer write closed fd=" << fd_);
		return;
	}

	if (connected_ != CONNECTED) {
		LOG_TRACE("tcp stream buffer filter not connected fd=" << fd_);
		return;
	}

	pending_writes_.insert(pending_writes_.end(), write_list.begin(), write_list.end());

	//TODO --- starvation handling.

	while (!pending_writes_.empty()) {
		auto data_view = pending_writes_.front();
		auto has_more = pending_writes_.size() > 1;

		auto write_ret = send(fd_, data_view->GetData(), data_view->GetDataLength(), (has_more ? MSG_MORE : 0) | MSG_NOSIGNAL | MSG_DONTWAIT);
		if (write_ret == -1) {
			LOG_TRACE("tcp stream buffer filter write error fd=" << fd_ << " error=" << std::strerror(errno));
			switch (errno) {
			case EAGAIN:
				LOG_DEBUG("tcp stream buffer filter write would block fd=" << fd_);
				return;
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
				LOG_DEBUG("tcp stream buffer filter write no longer writable fd=" << fd_ << " error=" << std::strerror(errno));
				write_closed_ = true;
				return;
			}
		}

		LOG_DEBUG("tcp stream buffer filter write " << write_ret << " bytes fd=" << fd_);

		if (write_ret == 0) {
			throw "write return 0 (kernel bug?)";
		}

		pending_writes_.pop_front();

		std::shared_ptr<const DataView> suffix_data_view;
		if (write_ret < data_view->GetDataLength()) {
			pending_writes_.push_front(data_view->Slice(write_ret).Save());
			LOG_DEBUG("tcp stream buffer filter partial write " <<  suffix_data_view->GetDataLength( )<< " bytes left fd=" << fd_);
			return;
		}
	}
}

void TCPStreamBufferFilter::Read() {
	//TODO... read...
	if (read_closed_) {
		LOG_TRACE("tcp stream buffer filter read closed fd=" << fd_);
		return;
	}

	if (connected_ != CONNECTED) {
		LOG_TRACE("tcp stream buffer filter not connected fd=" << fd_);
		return;
	}

	std::uint8_t read_buf[128000];

	while (true) {
		auto read_ret_ = recv(fd_, read_buf, sizeof(read_buf), MSG_DONTWAIT);

		switch (read_ret_) {
		case 0:
			LOG_DEBUG("tcp stream buffer filter read EOF fd=" << fd_);
			read_closed_ = true;
			return;
		case -1:
			switch (errno) {
			case EAGAIN:
				LOG_DEBUG("tcp stream buffer filter read would block fd=" << fd_);
				return;
			case EFAULT:
			case EINVAL:
			case ENOTCONN:
			case ENOTSOCK:
			case EBADF:
				throw std::system_error(errno, std::system_category(), "read failed");
			default:
				LOG_DEBUG("tcp stream buffer filter no longer readable fd=" << fd_ << " error=" << std::strerror(errno));
				read_closed_ = true;
			}
			return;
		default:
			LOG_DEBUG("stream buffer read " << read_ret_ << " bytes fd=" << fd_);
			DataView data_view(read_buf, read_ret_);
			HandleData(data_view);
		}
	}
}

void TCPStreamBufferFilter::Close() {
	if (read_closed_ && write_closed_) {
		return;
	}

	auto next = Next();

	if (next) {
		Close();

		if (!next->IsReadClosed() || !next->IsWriteClosed()) {
			return;
		}
	}

	read_closed_ = true;
	write_closed_ = true;
}

void TCPStreamBufferFilter::Connect() {
	LOG_TRACE("tcp stream buffer filter connect fd=" << fd_);

	if (connected_ == CONNECTED) {
		Next()->Connect();
		return;
	}

	if (connected_ == CONNECTED_BUT_NOT_NOTIFIED) {
		connected_ = CONNECTED;
		return;
	}

	int socket_error;
	socklen_t socket_error_len = sizeof(socket_error);

	if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) < 0) {
		throw std::system_error(errno, std::system_category(), "getsockopt failed");
	}

	if (socket_error != 0) {
		LOG_DEBUG("tcp stream buffer filter connect - failed on socket error fd=" << fd_ << " socket_error=" << std::strerror(socket_error));
		read_closed_ = true;
		write_closed_ = true;
		return;
	}

	LOG_DEBUG("tcp stream buffer filter connect - connected fd=" << fd_);

	connected_ = CONNECTED;
}

int TCPStreamBufferFilter::GetFlags() const {
	if (connected_ != NOT_CONNECTED) {
		return READ_FLAG | WRITE_FLAG | STREAM_FLAG;
	} else {
		if (read_closed_ && write_closed_) {
			return CLOSE_FLAG;
		} else {
			return WRITE_FLAG | STREAM_FLAG;
		}
	}
}

bool TCPStreamBufferFilter::IsReadClosed() const {
	return read_closed_;
}

bool TCPStreamBufferFilter::IsWriteClosed() const {
	return write_closed_;
}

bool TCPStreamBufferFilter::IsConnected() const {
	return connected_== CONNECTED;
}

} /* namespace ael */
