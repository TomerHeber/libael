/*
 * stream_buffer.cc
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#include <sys/epoll.h>

#include <arpa/inet.h>

#include <unistd.h>

#include <cstring>

#include "stream_buffer.h"
#include "tcp_stream_buffer_filter.h"
#include "async_io.h"
#include "log.h"
#include "config.h"

namespace ael {

StreamBuffer::StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd) :
		EventHandler(fd),
		stream_buffer_handler_(stream_buffer_handler),
		add_filter_allowed_(true),
		eof_called_(false),
		should_close_(false) {
	LOG_TRACE("stream buffer created id=" << id_ << " fd=" << fd);
}

std::shared_ptr<StreamBuffer> StreamBuffer::Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd) {
	std::shared_ptr<StreamBuffer> stream_buffer(new StreamBuffer(stream_buffer_handler, fd));
	auto tcp_stream_buffer_filter = TCPStreamBufferFilter::Create(stream_buffer, fd, true);
	stream_buffer->AddStreamBufferFilter(tcp_stream_buffer_filter);
	return stream_buffer;
}

std::shared_ptr<StreamBuffer> StreamBuffer::Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port) {
	sockaddr *addr = nullptr;
	socklen_t addr_len;
	sa_family_t domain;

	sockaddr_in in4 = {};
	sockaddr_in6 in6 = {};

	auto is_connected = false;

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

	LOG_TRACE("creating stream buffer - connecting fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);

	auto connect_ret = connect(fd, addr, addr_len);
	if (connect_ret == 0) {
		LOG_TRACE("creating stream buffer - connected fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
		is_connected = true;
	} else {
		switch(errno) {
		case EINPROGRESS:
			LOG_TRACE("creating stream buffer - connecting in progress fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
			break;
		case EAFNOSUPPORT:
		case EALREADY:
		case EBADF:
		case EFAULT:
		case EISCONN:
		case ENOTSOCK:
			throw std::system_error(errno, std::system_category(), "connect failed");
		default:
			LOG_WARN("creating stream buffer - failed to connect fd=" << fd << " ip_addr=" << ip_addr << " port=" << port << " error=" << std::strerror(errno));
		}
	}

	std::shared_ptr<StreamBuffer> stream_buffer(new StreamBuffer(stream_buffer_handler, fd));
	auto tcp_stream_buffer_filter = TCPStreamBufferFilter::Create(stream_buffer, fd, is_connected);
	stream_buffer->AddStreamBufferFilter(tcp_stream_buffer_filter);
	return stream_buffer;
}

void StreamBuffer::AddStreamBufferFilter(std::shared_ptr<StreamBufferFilter> stream_filter) {
	if (!add_filter_allowed_) {
		throw "filter added when it is not allowed";
	}

	LOG_DEBUG("attaching filter id=" << id_);

	add_filter_allowed_ = false;

	if (!stream_filters_.empty()) {
		auto prev_it = stream_filters_.back();
		auto prev = prev_it.get();
		prev->next_ = stream_filter.get();
		stream_filter->prev_ = prev;
	}

	stream_filters_.push_back(stream_filter);

	if (stream_filters_.size() > 1) {
		ModifyEvent();
	}
}

void StreamBuffer::Write(const DataView &data_view) {
	if (data_view.GetDataLength() == 0) {
		LOG_WARN("trying to write 0 data id=" << id_);
		return;
	}

	if (should_close_) {
		LOG_DEBUG("should close cannot write id=" << id_);
		return;
	}

	LOG_DEBUG("add write " << data_view.GetDataLength() << " bytes id=" << id_);

	auto saved_data_view = data_view.Save();

	pending_writes_lock_.lock();
	auto send_write_ready = pending_writes_.empty();
	pending_writes_.push_back(saved_data_view);
	pending_writes_lock_.unlock();

	if (send_write_ready) {
		ReadyEvent(WRITE_FLAG);
	}
}

void StreamBuffer::Close() {
	LOG_DEBUG("close invoked id=" << id_);
	should_close_ = true;
	ReadyEvent(CLOSE_FLAG);
}

void StreamBuffer::Handle(std::uint32_t events) {
	LOG_TRACE("handling events id=" << id_ << " events=" << events);

	auto stream_buffer_handler = stream_buffer_handler_.lock();
	if (!stream_buffer_handler) {
		LOG_WARN("stream buffer handler has been destroyed - closing event id=" << id_);
		CloseEvent();
		return;
	}

	if (events & (EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
		DoRead();
	}

	DoWrite();

	if (should_close_) {
		DoClose();
	} else if (!IsConnected()) {
		DoConnect(stream_buffer_handler);
	}

	DoFinalize(stream_buffer_handler);
}

void StreamBuffer::DoRead() {
	LOG_TRACE("read id=" << id_);

	auto filter = stream_filters_.front();

	if (filter->IsReadClosed()) {
		LOG_TRACE("read closed id=" << id_);
		if (!should_close_) {
			should_close_ = true;
		}
		return;
	}

	filter->Read();

	if (filter->IsReadClosed()) {
		LOG_TRACE("read closed id=" << id_);
		if (!should_close_) {
			should_close_ = true;
		}
		return;
	}
}

void StreamBuffer::DoWrite() {
	LOG_TRACE("write id=" << id_);

	auto filter = stream_filters_.back();

	if (filter->IsWriteClosed()) {
		LOG_TRACE("write closed id=" << id_);
		return;
	}

	std::list<std::shared_ptr<const DataView>> pending_writes_swap;
	pending_writes_lock_.lock();
	pending_writes_.swap(pending_writes_swap);
	pending_writes_lock_.unlock();

	filter->Write(pending_writes_swap);
}

void StreamBuffer::DoClose() {
	LOG_TRACE("close id=" << id_);
	stream_filters_.back()->Close();
}

void StreamBuffer::DoConnect(std::shared_ptr<StreamBufferHandler> stream_buffer_handler) {
	LOG_TRACE("connect id=" << id_);
	stream_filters_.back()->Connect();
	if (IsConnected()) {
		LOG_DEBUG("connect complete id=" << id_);
		add_filter_allowed_ = true;
		stream_buffer_handler->HandleConnected(shared_from_this());
		add_filter_allowed_ = false;
		ModifyEvent();
		ReadyEvent(READ_FLAG | WRITE_FLAG);
	}
}

void StreamBuffer::DoFinalize(std::shared_ptr<StreamBufferHandler> stream_buffer_handler) {
	LOG_TRACE("finalize id=" << id_);

	auto filter = stream_filters_.front();

	if (filter->IsReadClosed() && filter->IsWriteClosed()) {
		if (!eof_called_) {
			LOG_TRACE("EOF id=" << id_);
			eof_called_ = true;
			stream_buffer_handler->HandleEOF(shared_from_this());
			CloseEvent();
		}
		return;
	}
}

int StreamBuffer::GetFlags() const {
	return stream_filters_.back()->GetFlags();
}

void StreamBufferFilter::HandleData(const DataView &data_view) {
	auto stream_buffer = stream_buffer_.lock();
	if (!stream_buffer) {
		LOG_WARN("stream buffer has been destroyed");
		return;
	}

	auto stream_buffer_handler = stream_buffer->stream_buffer_handler_.lock();
	if (!stream_buffer_handler) {
		LOG_WARN("stream buffer handler has been destroyed");
		return;
	}

	stream_buffer_handler->HandleData(stream_buffer, data_view);
}

}
