/*
 * stream_buffer.cc
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "stream_buffer.h"
#include "async_io.h"
#include "log.h"
#include "config.h"

namespace ael {

std::shared_ptr<StreamBuffer> StreamBuffer::Create(int fd, std::shared_ptr<StreamBufferHandler> stream_buffer_handler) {
	LOG_INFO("creating a stream buffer fd=" << fd );
	return std::shared_ptr<StreamBuffer>(new StreamBuffer(fd, stream_buffer_handler));
}

StreamBuffer::StreamBuffer(int fd, std::shared_ptr<StreamBufferHandler> stream_buffer_handler) :
		EventHandler(fd),
		stream_buffer_handler_(stream_buffer_handler),
		read_closed_(false),
		write_closed_(false),
		eof_called_(false) {}

int StreamBuffer::GetFlags() const {
	return READ_FLAG | WRITE_FLAG | STREAM_FLAG;
}

void StreamBuffer::Handle(std::uint32_t events) {
	LOG_TRACE("stream buffer handle event_id=" << event_->GetID() << " event_fd=" << event_->GetFD() << " events=" << events);

	if (read_closed_ && write_closed_) {
		LOG_TRACE("stream buffer handle - read and write closed event_id=" << event_->GetID() << " event_fd=" << event_->GetFD() << " events=" << events);
		return;
	}

	auto stream_buffer_handler = stream_buffer_handler_.lock();
	if (!stream_buffer_handler) {
		LOG_WARN("stream buffer handler has been destroyed - closing event_id=" << event_->GetID() << " event_fd=" << event_->GetFD());
		event_->Close();
		return;
	}

	DoRead(events, stream_buffer_handler.get());
	DoWrite(events);
	DoFinalize(stream_buffer_handler.get());
}

void StreamBuffer::DoRead(std::uint32_t events, StreamBufferHandler *stream_buffer_handler) {
	if (read_closed_) {
		LOG_TRACE("stream buffer - read closed fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	if (!(events & (EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP))) {
		LOG_TRACE("stream buffer - no relevant events fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	LOG_TRACE("stream buffer - trying to read fd=" << event_->GetFD() << " id=" << event_->GetID());

	std::uint8_t read_buf[128000];
	auto read_total = 0;

	while (true) {
		auto read_ret_ = recv(event_->GetFD(), read_buf, sizeof(read_buf), MSG_DONTWAIT);

		switch (read_ret_) {
		case 0:
			LOG_DEBUG("stream buffer - read EOF fd=" << event_->GetFD() << " id=" << event_->GetID());
			read_closed_ = true;
			return;
		case -1:
			DoReadHandleError();
			return;
		default:
			LOG_DEBUG("stream buffer - read " << read_ret_ << " bytes fd=" << event_->GetFD() << " id=" << event_->GetID());

			DataView data_view(read_buf, read_ret_);
			stream_buffer_handler->HandleData(shared_from_this(), data_view);

			read_total += data_view.GetDataLength();
			if (read_total >= Config::_config.read_starvation_limit_) {
				LOG_DEBUG("stream buffer - reached read starvation limit fd=" << event_->GetFD() << " id=" << event_->GetID());
				event_->Ready(READ_FLAG);
				return;
			}
		}
	}
}

void StreamBuffer::DoReadHandleError() {
	LOG_TRACE("stream buffer - read error fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(errno));

	switch (errno) {
	case EAGAIN:
		return;
	case EFAULT:
	case EINVAL:
	case ENOTCONN:
	case ENOTSOCK:
	case EBADF:
		throw std::system_error(errno, std::system_category(), "read failed");
	default:
		LOG_DEBUG("stream buffer - no longer readable fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(errno));
		read_closed_ = true;
	}
}

void StreamBuffer::DoWrite(std::uint32_t events) {
	if (write_closed_) {
		LOG_TRACE("stream buffer - write closed fd=" << event_->GetFD() << " id=" << event_->GetID());
		return;
	}

	if (!(events & (EPOLLOUT | EPOLLRDHUP | EPOLLERR | EPOLLHUP))) {
		LOG_TRACE("stream buffer - no relevant events fd=" << event_->GetFD() << " id=" << event_->GetID());
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
			LOG_TRACE("stream buffer - nothing to write events fd=" << event_->GetFD() << " id=" << event_->GetID());
			return;
		}

		auto write_ret = send(event_->GetFD(), data_view->GetData(), data_view->GetDataLength(), (has_more_flag ? MSG_MORE : 0) | MSG_NOSIGNAL | MSG_DONTWAIT);

		if (write_ret == -1) {
			DoWriteHandleError();
			return;
		}

		LOG_DEBUG("stream buffer - write " << write_ret << " bytes fd=" << event_->GetFD() << " id=" << event_->GetID());

		std::shared_ptr<const DataView> suffix_data_view;
		if (write_ret > 0 && write_ret < data_view->GetDataLength()) {
			suffix_data_view = data_view->Slice(write_ret).Save();
			LOG_DEBUG("stream buffer - partial write " <<  suffix_data_view->GetDataLength( )<< " bytes left fd=" << event_->GetFD() << " id=" << event_->GetID());
		}


		lock_.lock();
		pending_writes_.pop_front();
		if (suffix_data_view) {
			pending_writes_.push_front(suffix_data_view);
		}
		lock_.unlock();

		write_total += write_ret;
		if (write_total >= Config::_config.write_starvation_limit_) {
			LOG_DEBUG("stream buffer - reached write starvation limit fd=" << event_->GetFD() << " id=" << event_->GetID());
			event_->Ready(WRITE_FLAG);
			return;
		}
	}
}

void StreamBuffer::DoWriteHandleError() {
	LOG_TRACE("stream buffer - write error fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(errno));

	switch (errno) {
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
		throw std::system_error(errno, std::system_category(), "write failed");
	default:
		LOG_DEBUG("stream buffer - no longer writable fd=" << event_->GetFD() << " id=" << event_->GetID() << " error=" << std::strerror(errno));
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
				LOG_DEBUG("stream buffer - no longer writable read closed and nothing to write fd=" << event_->GetFD() << " id=" << event_->GetID())
			}
		}
	}

	if (read_closed_ && write_closed_ && !eof_called_) {
		eof_called_ = true;
		LOG_DEBUG("stream buffer - EOF fd=" << event_->GetFD() << " id=" << event_->GetID())
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
		LOG_TRACE("stream buffer - new data to write - sending write ready fd=" << event_->GetFD() << " id=" << event_->GetID());
		event_->Ready(WRITE_FLAG);
	}

}

void StreamBuffer::Close() {
	bool desired = false;

	if (read_closed_.compare_exchange_strong(desired, true)) {
		event_->Ready(WRITE_FLAG);
	}
}

}
