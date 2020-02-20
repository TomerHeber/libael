/*
 * stream_buffer.cc
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

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

StreamBuffer::StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, Handle handle, StreamBufferMode mode) :
		EventHandler(handle),
		stream_buffer_handler_(stream_buffer_handler),
		add_filter_allowed_(true),
		eof_called_(false),
		should_close_(false),
		mode_(mode) {
	LOG_TRACE("stream buffer created " << this);
}


std::shared_ptr<StreamBuffer> StreamBuffer::Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, Handle handle, StreamBufferMode mode) {
	std::shared_ptr<StreamBuffer> stream_buffer(new StreamBuffer(stream_buffer_handler, handle, mode));
	auto tcp_stream_buffer_filter = TCPStreamBufferFilter::Create(stream_buffer, handle, true);
	stream_buffer->AddStreamBufferFilter(tcp_stream_buffer_filter);
	return stream_buffer;
}

std::shared_ptr<StreamBuffer> StreamBuffer::CreateForServer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, Handle handle) {
	return Create(stream_buffer_handler, handle, SERVER_MODE);
}

std::shared_ptr<StreamBuffer> StreamBuffer::CreateForClient(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, Handle handle) {
	return Create(stream_buffer_handler, handle, CLIENT_MODE);
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
}

void StreamBuffer::HandleEvents(Handle handle, std::uint32_t events) {
	std::ignore = handle;

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
		DoRead();
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

	if (filter->read_closed_) {
		LOG_TRACE("filter read closed " << filter);
		return;
	}

	filter->Read();
}

void StreamBuffer::DoWrite() {
	LOG_TRACE("write " << this);

	auto filter = stream_filters_.back();

	if (filter->write_closed_) {
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
		if (filter->write_closed_) {
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
		if (!filter->read_closed_ || !filter->write_closed_) {
			LOG_TRACE("calling close on filter " << filter);
			filter->Close();
		}

		if (!filter->read_closed_ || !filter->write_closed_) {
			LOG_TRACE("close on filter - delayed " << filter << " filter_read_closed=" << filter->read_closed_ << " filter_write_closed=" << filter->write_closed_);
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
	} else {
		LOG_TRACE((mode_ == CLIENT_MODE ? "connect" : "accept") << " pending " << this);
	}
}

void StreamBuffer::DoFinalize(std::shared_ptr<StreamBufferHandler> stream_buffer_handler) {
	LOG_TRACE("finalize " << this);

	auto filter = stream_filters_.back();

	if (!should_close_ && filter->read_closed_) {
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
	return stream_filters_.back()->connected_;
}

bool StreamBuffer::IsReadClosed() const {
	for (auto filter_it = stream_filters_.rbegin(); filter_it != stream_filters_.rend(); ++filter_it) {
		auto filter = *filter_it;
		if (!filter->read_closed_) {
			LOG_TRACE("read is not closed - found a filter open for read " << filter);
			return false;
		}
	}

	LOG_TRACE("read is closed " << this);
	return true;
}

bool StreamBuffer::IsWriteClosed() const {
	for (auto filter : stream_filters_) {
		if (!filter->write_closed_) {
			LOG_TRACE("write is not closed found a filter open for write " << filter);
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

StreamBufferFilter::StreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer) :
		connected_(false),
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

	while (!pending_out_.empty()) {
		auto data_view = pending_out_.front();
		pending_out_.pop_front();

		auto out_result = Out(data_view);

		if (out_result.ShouldCloseWrite()) {
			write_closed_ = true;
			return;
		}

		if (data_view) {
			pending_out_.push_front(data_view);
			return;
		}
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
	LOG_TRACE("close " << this << " write_closed=" << write_closed_ << " pending_out=" << !pending_out_.empty())

	if (pending_out_.empty() || write_closed_) {
		if (Shutdown().IsComplete()) {
			LOG_TRACE("shutdown is complete " << this)
			write_closed_ = true;
			read_closed_ = true;
		}
		return;
	}

	LOG_TRACE("flushing pending out data " << this)

	while (!pending_out_.empty()) {
		auto data_view = pending_out_.front();
		pending_out_.pop_front();

		auto out_result = Out(data_view);

		if (out_result.ShouldCloseWrite()) {
			write_closed_ = true;
			break;
		}

		if (data_view) {
			pending_out_.push_front(data_view);
			break;
		}
	}

	if (pending_out_.empty() || write_closed_) {
		if (Shutdown().IsComplete()) {
			LOG_TRACE("shutdown is complete" << this)
			write_closed_ = true;
			read_closed_ = true;
		}
		return;
	}

	LOG_TRACE("cannot close more data to flush out " << this << " write_closed=" << write_closed_ << " pending_out=" << !pending_out_.empty())
}

void StreamBufferFilter::HandleData(const std::shared_ptr<const DataView> &data_view) {
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
	if (connected_ || order_ > 0) {
		return READ_FLAG | WRITE_FLAG | STREAM_FLAG;
	} else {
		return WRITE_FLAG | STREAM_FLAG;
	}
}

}
