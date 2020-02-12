/*
 * event.cc
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include "event_loop.h"
#include "event.h"
#include "log.h"

#include <unistd.h>

#include <atomic>

namespace ael {

std::atomic_uint64_t id_counter(0);

Event::Event(std::shared_ptr<EventLoop> event_loop, std::shared_ptr<EventHandler> event_handler) :
		event_loop_(event_loop),
		event_handler_(event_handler),
		fd_(event_handler->GetFD()) {
	id_ = id_counter.fetch_add(1);
	LOG_TRACE("event is created id=" << id_);
}

Event::~Event() {
	LOG_TRACE("event is destroyed id=" << id_);

	if (fd_ >= 0) {
		close(fd_);
	}
}

int Event::GetFlags() const {
	auto event_handler = event_handler_.lock();
	if (!event_handler) {
		LOG_WARN("event get flags called but event handler deleted id=" << id_ << " fd=" << fd_);
		return 0;
	}

	return event_handler->GetFlags();
}

void Event::Close() {
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		std::call_once(close_flag_, &EventLoop::Remove, event_loop, id_);
	}
}

void Event::Ready(int flags) {
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		event_loop->Ready(shared_from_this(), flags);
	} else {
		LOG_WARN("event ready called but event loop deleted id=" << id_ << " fd=" << fd_);
	}
}

void Event::Modify() {
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		event_loop->Modify(shared_from_this());
	} else {
		LOG_WARN("event modify called but event loop deleted id=" << id_ << " fd=" << fd_);
	}
}

EventHandler::EventHandler(int fd) : fd_(fd) {
	LOG_TRACE("event handler is created fd=" << fd);
}

int EventHandler::GetFD() const {
	return fd_;
}

EventHandler::~EventHandler() {
	LOG_TRACE("event handler is destroyed");
	if (event_) {
		LOG_TRACE("event handler is destroyed - calling event_->Close() event_id=" << event_->GetID() << " event_fd=" << event_->GetFD());
		event_->Close();
	} else if (fd_ >= 0) {
		LOG_TRACE("event handler is destroyed - event never attached closing file descriptor fd_=" << fd_);
		close(fd_);
	}
}

}
