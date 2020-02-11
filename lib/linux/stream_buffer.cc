/*
 * stream_buffer.cc
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "stream_buffer.h"
#include "async_io.h"
#include "log.h"
#include "config.h"

namespace ael {

std::shared_ptr<StreamBuffer> StreamBuffer::Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd) {
	LOG_DEBUG("creating a stream buffer fd=" << fd );
	return std::shared_ptr<StreamBuffer>(new StreamBuffer(stream_buffer_handler, fd, true));
}

std::shared_ptr<StreamBuffer> StreamBuffer::Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port) {
	LOG_DEBUG("creating a stream buffer ip_addr=" << ip_addr << " port=" << port);

	sockaddr *addr = nullptr;
	socklen_t addr_len;
	sa_family_t domain;

	sockaddr_in in4 = {};
	sockaddr_in6 in6 = {};

	if (inet_pton(AF_INET, ip_addr.c_str(), &in4.sin_addr) == 1) {
		in4.sin_family = AF_INET;
		in4.sin_port = htons(port);
		addr = reinterpret_cast<sockaddr*>(&in4);
		addr_len = sizeof(in4);
		domain = AF_INET;
	} else if (inet_pton(AF_INET6, ip_addr.c_str(), &in6.sin6_addr) == 1) {
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons(port);
		addr = reinterpret_cast<sockaddr*>(&in6);
		addr_len = sizeof(in6);
		domain = AF_INET6;
	} else {
		throw "invalid host - inet_pton failed for both IPv4 and IPv6";
	}

	auto fd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "socket failed");
	}

	LOG_TRACE("creating a stream buffer - connecting fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);

	auto connect_ret = connect(fd, addr, addr_len);
	if (connect_ret == 0) {
		LOG_TRACE("creating a stream buffer - connected fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
		return std::shared_ptr<StreamBuffer>(new StreamBuffer(stream_buffer_handler, fd, true));
	}

	switch(errno) {
	case EINPROGRESS:
		LOG_TRACE("creating a stream buffer - still connecting fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
		return std::shared_ptr<StreamBuffer>(new StreamBuffer(stream_buffer_handler, fd, false));
	case EAFNOSUPPORT:
	case EALREADY:
	case EBADF:
	case EFAULT:
	case EISCONN:
	case ENOTSOCK:
		throw std::system_error(errno, std::system_category(), "connect failed");
	default:
		LOG_WARN("creating a stream buffer - failed to connect fd=" << fd << " ip_addr=" << ip_addr << " port=" << port << " error=" << std::strerror(errno));
		// <comment1> Will create a CLOSE_FLAG status - extra work so EOF callback get executed only in the context of an attached event loop.
		auto stream_buffer = std::shared_ptr<StreamBuffer>(new StreamBuffer(stream_buffer_handler, fd, false));
		stream_buffer->write_closed_ = true;
		stream_buffer->read_closed_ = true;
		return stream_buffer;
	}
}

StreamBuffer::StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd, bool connected) :
		EventHandler(fd),
		stream_buffer_handler_(stream_buffer_handler),
		read_closed_(false),
		write_closed_(false),
		eof_called_(false),
		connected_(connected) {}

int StreamBuffer::GetFlags() const {
	if (connected_) {
		return READ_FLAG | WRITE_FLAG | STREAM_FLAG;
	} else {
		if (read_closed_ && write_closed_) {
			// this is required because of <comment1>.
			return CLOSE_FLAG;
		} else {
			return WRITE_FLAG | STREAM_FLAG;
		}
	}
}

void StreamBuffer::Handle(std::uint32_t events) {
	LOG_TRACE("stream buffer handle event_id=" << event_->GetID() << " event_fd=" << event_->GetFD() << " events=" << events);

	if (read_closed_ && write_closed_) {
		LOG_TRACE("stream buffer handle read and write closed event_id=" << event_->GetID() << " event_fd=" << event_->GetFD() << " events=" << events);
		return;
	}

	auto stream_buffer_handler = stream_buffer_handler_.lock();
	if (!stream_buffer_handler) {
		LOG_WARN("stream buffer handler has been destroyed - closing event_id=" << event_->GetID() << " event_fd=" << event_->GetFD());
		event_->Close();
		return;
	}

	DoRead(events, stream_buffer_handler.get());
	DoWrite(events, stream_buffer_handler.get());

	DoFinalize(stream_buffer_handler.get());
}

void StreamBuffer::DoRead(std::uint32_t events, StreamBufferHandler *stream_buffer_handler) {
	if (read_closed_) {
		LOG_TRACE("stream buffer read closed fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	if (!(events & (EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP))) {
		LOG_TRACE("stream buffer no relevant events fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	LOG_TRACE("stream buffer trying to read fd=" << event_->GetFD() << " id=" << event_->GetID());

	std::uint8_t read_buf[128000];
	auto read_total = 0;

	while (true) {
		auto read_ret_ = recv(event_->GetFD(), read_buf, sizeof(read_buf), MSG_DONTWAIT);

		switch (read_ret_) {
		case 0:
			LOG_DEBUG("stream buffer read EOF fd=" << event_->GetFD() << " id=" << event_->GetID());
			read_closed_ = true;
			return;
		case -1:
			DoReadHandleError(errno);
			return;
		default:
			LOG_DEBUG("stream buffer read " << read_ret_ << " bytes fd=" << event_->GetFD() << " id=" << event_->GetID());

			DataView data_view(read_buf, read_ret_);
			stream_buffer_handler->HandleData(shared_from_this(), data_view);

			read_total += data_view.GetDataLength();
			if (read_total >= Config::_config.read_starvation_limit_) {
				LOG_DEBUG("stream buffer reached read starvation limit fd=" << event_->GetFD() << " id=" << event_->GetID());
				event_->Ready(READ_FLAG);
				return;
			}
		}
	}
}

void StreamBuffer::DoReadHandleError(int error) {
	LOG_TRACE("stream buffer read error fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(error));

	switch (error) {
	case EAGAIN:
		return;
	case EFAULT:
	case EINVAL:
	case ENOTCONN:
	case ENOTSOCK:
	case EBADF:
		throw std::system_error(error, std::system_category(), "read failed");
	default:
		LOG_DEBUG("stream buffer no longer readable fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(error));
		read_closed_ = true;
	}
}

void StreamBuffer::DoWrite(std::uint32_t events, StreamBufferHandler *stream_buffer_handler) {
	if (write_closed_) {
		LOG_TRACE("stream buffer write closed fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	if (!(events & (EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP))) {
		LOG_TRACE("stream buffer no relevant events fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	if (!connected_) {
		CompleteConnect(events, stream_buffer_handler);
		return;
	}

	auto write_total = 0;

	while (true) {
		std::shared_ptr<const DataView> data_view;
		auto has_more_flag = false;

		lock_.lock();
		if (!pending_writes_.empty()) {
			data_view = pending_writes_.front();
			has_more_flag = pending_writes_.size() > 2 && (data_view->GetDataLength() + write_total <= Config::_config.write_starvation_limit_);
		}
		lock_.unlock();

		if (!data_view) {
			LOG_TRACE("stream buffer nothing to write events fd=" << event_->GetFD() << " id=" << event_->GetID());
			return;
		}

		auto write_ret = send(event_->GetFD(), data_view->GetData(), data_view->GetDataLength(), (has_more_flag ? MSG_MORE : 0) | MSG_NOSIGNAL | MSG_DONTWAIT);
		if (write_ret == -1) {
			DoWriteHandleError(errno);
			return;
		}

		LOG_DEBUG("stream buffer write " << write_ret << " bytes fd=" << event_->GetFD() << " id=" << event_->GetID());

		std::shared_ptr<const DataView> suffix_data_view;
		if (write_ret > 0 && write_ret < data_view->GetDataLength()) {
			suffix_data_view = data_view->Slice(write_ret).Save();
			LOG_DEBUG("stream buffer partial write " <<  suffix_data_view->GetDataLength( )<< " bytes left fd=" << event_->GetFD() << " id=" << event_->GetID());
		}


		lock_.lock();
		pending_writes_.pop_front();
		if (suffix_data_view) {
			pending_writes_.push_front(suffix_data_view);
		}
		lock_.unlock();

		write_total += write_ret;
		if (write_total >= Config::_config.write_starvation_limit_) {
			LOG_DEBUG("stream buffer reached write starvation limit fd=" << event_->GetFD() << " id=" << event_->GetID());
			event_->Ready(WRITE_FLAG);
			return;
		}
	}
}

void StreamBuffer::DoWriteHandleError(int error) {
	LOG_TRACE("stream buffer write error fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(error));

	switch (error) {
	case EAGAIN:
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
		throw std::system_error(error, std::system_category(), "write failed");
	default:
		LOG_DEBUG("stream buffer no longer writable fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(error));
		write_closed_ = true;
	}
}

void StreamBuffer::DoFinalize(StreamBufferHandler *stream_buffer_handler) {
	if (read_closed_) {
		if (!write_closed_) {
			lock_.lock();
			auto has_no_pending_writes = pending_writes_.empty();
			lock_.unlock();
			if (has_no_pending_writes) {
				write_closed_ = true; // If writes come after this, they will not be written.
				LOG_DEBUG("stream buffer no longer writable read closed and nothing to write fd=" << event_->GetFD() << " id=" << event_->GetID())
			}
		}
	}

	if (write_closed_ && !connected_) {
		read_closed_ = true;
	}

	if (read_closed_ && write_closed_ && !eof_called_) {
		eof_called_ = true;
		LOG_DEBUG("stream buffer EOF fd=" << event_->GetFD() << " id=" << event_->GetID())
		stream_buffer_handler->HandleEOF(shared_from_this());
		event_->Close();
	}
}

void StreamBuffer::Write(const DataView &data_view) {
	if (data_view.GetDataLength() == 0) {
		LOG_WARN("trying to write 0 data (empty data view) fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	auto saved_data_view = data_view.Save();

	lock_.lock();
	auto send_write_ready = pending_writes_.empty();
	pending_writes_.push_back(saved_data_view);
	lock_.unlock();

	if (send_write_ready) {
		LOG_TRACE("stream buffer new data to write - sending write ready fd=" << event_->GetFD() << " id=" << event_->GetID());
		event_->Ready(WRITE_FLAG);
	}

}

void StreamBuffer::CompleteConnect(std::uint32_t events, StreamBufferHandler *stream_buffer_handler) {
	LOG_TRACE("stream buffer connect complete fd=" << event_->GetFD() << " id=" << event_->GetID());

	if (!(events & EPOLLOUT)) {
		LOG_WARN("stream buffer connect complete - no epollout consider failed fd=" << event_->GetFD() << " id=" << event_->GetID() << " events=" << events);
		write_closed_ = true;
		return;
	}

	int socket_error;
	socklen_t socket_error_len = sizeof(socket_error);

	if (getsockopt(event_->GetFD(), SOL_SOCKET, SO_ERROR, &socket_error, &socket_error_len) < 0) {
		throw std::system_error(errno, std::system_category(), "getsockopt failed");
	}

	if (socket_error != 0) {
		LOG_DEBUG("stream buffer connect complete - failed on socket error fd=" << event_->GetFD() << " id=" << event_->GetID() << " socket_error=" << std::strerror(socket_error));
		write_closed_ = true;
		return;
	}

	LOG_DEBUG("stream buffer connect complete - connected success fd=" << event_->GetFD() << " id=" << event_->GetID());

	connected_ = true;

	event_->Modify();

	stream_buffer_handler->HandleConnected(shared_from_this());
}

void StreamBuffer::Close() {
	bool desired = false;

	if (read_closed_.compare_exchange_strong(desired, true)) {
		event_->Ready(WRITE_FLAG);
	}
}

}
