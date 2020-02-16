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

std::ostream& operator<<(std::ostream &out, const StreamBuffer *stream_buffer) {
	const EventHandler *event_hadler = stream_buffer;
	out << event_hadler;
	return out;
}

StreamBuffer::StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd, StreamBufferMode mode) :
		EventHandler(fd),
		stream_buffer_handler_(stream_buffer_handler),
		add_filter_allowed_(true),
		eof_called_(false),
		should_close_(false),
		mode_(mode) {
	LOG_TRACE("stream buffer created " << this);
}


std::shared_ptr<StreamBuffer> StreamBuffer::Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd, StreamBufferMode mode) {
	std::shared_ptr<StreamBuffer> stream_buffer(new StreamBuffer(stream_buffer_handler, fd, mode));
	auto tcp_stream_buffer_filter = TCPStreamBufferFilter::Create(stream_buffer, fd, true);
	stream_buffer->AddStreamBufferFilter(tcp_stream_buffer_filter);
	return stream_buffer;
}

std::shared_ptr<StreamBuffer> StreamBuffer::CreateForServer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd) {
	return Create(stream_buffer_handler, fd, SERVER_MODE);
}

std::shared_ptr<StreamBuffer> StreamBuffer::CreateForClient(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd) {
	return Create(stream_buffer_handler, fd, CLIENT_MODE);
}

std::shared_ptr<StreamBuffer> StreamBuffer::CreateForClient(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port) {
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

	std::shared_ptr<StreamBuffer> stream_buffer(new StreamBuffer(stream_buffer_handler, fd, CLIENT_MODE));
	auto tcp_stream_buffer_filter = TCPStreamBufferFilter::Create(stream_buffer, fd, is_connected);
	stream_buffer->AddStreamBufferFilter(tcp_stream_buffer_filter);
	return stream_buffer;
}

void StreamBuffer::AddStreamBufferFilter(std::shared_ptr<StreamBufferFilter> stream_filter) {
	if (!add_filter_allowed_) {
		throw "filter added when it is not allowed";
	}

	add_filter_allowed_ = false;

	if (!stream_filters_.empty()) {
		auto prev_it = stream_filters_.back();
		auto prev = prev_it.get();
		prev->next_ = stream_filter.get();
		stream_filter->prev_ = prev;
		stream_filter->order_ = prev->order_ + 1;
	} else {
		stream_filter->order_ = 0;
	}

	LOG_DEBUG("attaching filter " << stream_filter);

	stream_filters_.push_back(stream_filter);

	if (stream_filters_.size() > 1) {
		ModifyEvent();
	}
}

void StreamBuffer::Handle(std::uint32_t events) {
	LOG_TRACE("handling events " << this << " events=" << events);

	auto stream_buffer_handler = stream_buffer_handler_.lock();
	if (!stream_buffer_handler) {
		LOG_WARN("stream buffer handler has been destroyed - closing " << this);
		CloseEvent();
		return;
	}

	if (should_close_) {
		DoClose();
	} else if (!IsConnected() ) {
		DoConnect(stream_buffer_handler);
	} else {
		if (events & (EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
			DoRead();
		}
		DoWrite();
	}

	DoFinalize(stream_buffer_handler);
}

void StreamBuffer::Write(const DataView &data_view) {
	if (data_view.GetDataLength() == 0) {
		LOG_WARN("trying to write 0 data " << this);
		return;
	}

	if (should_close_) {
		LOG_DEBUG("should close cannot write " << this);
		return;
	}

	LOG_DEBUG("add write " << data_view.GetDataLength() << " bytes " << this);

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
	LOG_DEBUG("close invoked " << this);
	should_close_ = true;
	ReadyEvent(CLOSE_FLAG);
}

void StreamBuffer::DoRead() {
	LOG_TRACE("read " << this);

	auto filter = stream_filters_.back();

	if (filter->IsReadClosed()) {
		LOG_TRACE("filter read closed " << filter);
		return;
	}

	filter->Read();
}

void StreamBuffer::DoWrite() {
	LOG_TRACE("write " << this);

	auto filter = stream_filters_.back();

	if (filter->IsWriteClosed()) {
		LOG_TRACE("filter write closed " << filter);
		return;
	}

	std::list<std::shared_ptr<const DataView>> pending_writes_swap;
	pending_writes_lock_.lock();
	if (!pending_writes_.empty()) {
		pending_writes_.swap(pending_writes_swap);
	}
	pending_writes_lock_.unlock();

	if (!pending_writes_swap.empty()) {
		filter->Write(pending_writes_swap);
	} else {
		LOG_TRACE("write - nothing to write " << this);
	}
}

void StreamBuffer::DoClose() {
	LOG_TRACE("close " << this);

	auto should_flush = true;
	for (auto filter : stream_filters_) {
		if (filter->IsWriteClosed()) {
			should_flush = false;
			break;
		}
	}

	if (should_flush) {
		LOG_TRACE("flushing write before close " << this);
		DoWrite();
	}

	for (auto filter_it = stream_filters_.rbegin(); filter_it != stream_filters_.rend(); ++filter_it) {
		auto filter = *filter_it;
		if (!filter->IsReadClosed() || !filter->IsWriteClosed()) {
			LOG_TRACE("calling close on filter " << filter);
			filter->Close();
		}

		if (!filter->IsReadClosed() || !filter->IsWriteClosed()) {
			LOG_TRACE("close on filter - delayed " << filter << " filter_read_closed=" << filter->IsReadClosed() << " filter_write_closed=" << filter->IsWriteClosed());
			break;
		}
	}
}

void StreamBuffer::DoConnect(std::shared_ptr<StreamBufferHandler> stream_buffer_handler) {
	auto filter = stream_filters_.back();

	ConnectResult connect_result;

	if (mode_ == CLIENT_MODE) {
		LOG_TRACE("connect " << this);
		connect_result = filter->Connect();
	} else {
		LOG_TRACE("accept " << this);
		connect_result = filter->Accept();
	}

	if (connect_result.IsFailed()) {
		LOG_DEBUG((mode_ == CLIENT_MODE ? "connect" : "accept") << " failed " << this);
		filter->read_closed_ = true;
		filter->write_closed_ = true;
	} else if (connect_result.IsSuccess()) {
		LOG_DEBUG((mode_ == CLIENT_MODE ? "connect" : "accept") << " complete " << this);
		filter->connected_ = true;
		add_filter_allowed_ = true;
		stream_buffer_handler->HandleConnected(shared_from_this());
		add_filter_allowed_ = false;
		ModifyEvent();
		ReadyEvent(READ_FLAG | WRITE_FLAG);
	} else {
		LOG_TRACE((mode_ == CLIENT_MODE ? "connect" : "accept") << " pending " << this);
	}
}

void StreamBuffer::DoFinalize(std::shared_ptr<StreamBufferHandler> stream_buffer_handler) {
	LOG_TRACE("finalize " << this);

	auto filter = stream_filters_.back();

	if (!should_close_ && filter->IsReadClosed()) {
		LOG_TRACE("filter is read closed " << filter);
		should_close_ = true;
		DoClose();
	}

	if (IsReadClosed() && IsWriteClosed()) {
		if (!eof_called_) {
			LOG_TRACE("EOF " << this);
			eof_called_ = true;
			stream_buffer_handler->HandleEOF(shared_from_this());
			CloseEvent();
		}
	}
}

bool StreamBuffer::IsConnected() const {
	return stream_filters_.back()->IsConnected();
}

bool StreamBuffer::IsReadClosed() const {
	for (auto filter_it = stream_filters_.rbegin(); filter_it != stream_filters_.rend(); ++filter_it) {
		auto filter = *filter_it;
		if (!filter->IsReadClosed()) {
			LOG_TRACE("found a filter open for read " << filter);
			return false;
		}
	}

	LOG_TRACE("read is closed " << this);
	return true;
}

bool StreamBuffer::IsWriteClosed() const {
	for (auto filter : stream_filters_) {
		if (!filter->IsWriteClosed()) {
			LOG_TRACE("found a filter open for write " << filter);
			return false;
		}
	}

	LOG_TRACE("write is closed " << this);
	return true;
}

int StreamBuffer::GetFlags() const {
	return stream_filters_.back()->GetFlags();
}

std::ostream& operator<<(std::ostream &out, const StreamBufferFilter *filter) {
	out << "id=" << filter->id_<< " order=" << filter->order_;
	return out;
}

StreamBufferFilter::StreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, bool connected) :
		connected_(connected),
		read_closed_(false),
		write_closed_(false),
		prev_(nullptr),
		next_(nullptr),
		stream_buffer_(stream_buffer),
		id_(stream_buffer->GetId()),
		order_(~1) {}

void StreamBufferFilter::Write(const std::list<std::shared_ptr<const DataView>> &write_list) {
	LOG_TRACE("write " << this);

	pending_out_.insert(pending_out_.end(), write_list.begin(), write_list.end());

	auto out_result = Out(pending_out_);

	if (out_result.ShouldCloseWrite()) {
		write_closed_ = true;
	}
}

void StreamBufferFilter::Read() {
	LOG_TRACE("read " << this);

	while (true) {
		auto in_result = In();

		if (in_result.ShouldCloseRead()) {
			read_closed_ = true;
			return;
		}

		if (!in_result.HasData()) {
			return;
		}

		auto data_view = in_result.GetData();
		HandleData(data_view);
	}
}

void StreamBufferFilter::Close() {
	LOG_TRACE("close " << this);

	read_closed_ = true;

	if (pending_out_.empty() || !IsConnected()) {
		LOG_TRACE("write closed " << this);
		write_closed_ = true;
		return;
	}

	auto out_result = Out(pending_out_);

	if (pending_out_.empty() || out_result.ShouldCloseWrite()) {
		LOG_TRACE("write closed after flush " << this);
		write_closed_ = true;
		return;
	}

	LOG_TRACE("cannot close more data to flush out " << this);
}

void StreamBufferFilter::HandleData(std::shared_ptr<const DataView> &data_view) {
	auto stream_buffer = stream_buffer_.lock();
	if (!stream_buffer) {
		LOG_WARN("stream buffer has been destroyed " << this);
		return;
	}

	auto stream_buffer_handler = stream_buffer->stream_buffer_handler_.lock();
	if (!stream_buffer_handler) {
		LOG_WARN("stream buffer handler has been destroyed " << this);
		return;
	}

	stream_buffer_handler->HandleData(stream_buffer, data_view);
}

int StreamBufferFilter::GetFlags() const {
	if (connected_ || order_ > 1) {
		return READ_FLAG | WRITE_FLAG | STREAM_FLAG;
	} else {
		return WRITE_FLAG | STREAM_FLAG;
	}
}

}
