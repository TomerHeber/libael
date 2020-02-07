/*
 * event.cc
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include "event.h"
#include "eventloop.h"
#include "log.h"

#include <atomic>

namespace ael {

std::atomic_uint64_t id_counter(0);

Event::Event(std::weak_ptr<EventLoop> event_loop, std::weak_ptr<EventHandler> event_handler, int fd, int flags) :
		event_loop_(event_loop),
		event_handler_(event_handler),
		fd_(fd),
		flags_(flags) {
	id_ = id_counter.fetch_add(1);
	LOG_TRACE("event is created id=" << id_);
}

void Event::Close() {
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		std::call_once(close_flag_, &EventLoop::Remove, event_loop, id_);
	}
}

void Event::Ready() {
	//TODO
}

EventHandler::EventHandler() {
	LOG_TRACE("event handler is created");
}

EventHandler::~EventHandler() {
	LOG_TRACE("event handler is destroyed");
	if (event_) {
		LOG_TRACE("event handler is destroyed - calling event_->Close() event_id=" << event_->GetID() << " event_fd=" << event_->GetFD());
		event_->Close();
	}
}

}
