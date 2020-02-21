/*
 * event.cc
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include "event_loop.h"
#include "event.h"
#include "log.h"

#include <atomic>

namespace ael {

static std::atomic_uint64_t id_counter(0);

Event::Event(std::shared_ptr<EventLoop> event_loop, std::shared_ptr<EventHandler> event_handler) :
		id_(event_handler->id_),
		event_loop_(event_loop),
		event_handler_(event_handler),
		handle_(event_handler->handle_) {
	LOG_TRACE("event is created " << this);
}

Event::~Event() {
	LOG_TRACE("event is destroyed " << this);

	if (handle_) {
		handle_.Close();
	}
}

std::ostream& operator<<(std::ostream &out, const Event *event) {
	out << "id=" << event->id_ << " handle=" << event->handle_;
	return out;
}

Events Event::GetEvents() const {
	auto event_handler = event_handler_.lock();
	if (!event_handler) {
		LOG_WARN("event get events called but event handler deleted " << this);
		return 0;
	}

	return event_handler->GetEvents();
}

void Event::Close() {
	LOG_TRACE("closing event " << this)
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		std::call_once(close_flag_, &EventLoop::Remove, event_loop, id_);
	}
}

void Event::Ready(Events events) {
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		event_loop->Ready(shared_from_this(), events);
	} else {
		LOG_WARN("event ready called but event loop deleted " << this);
	}
}

void Event::Modify() {
	auto event_loop = event_loop_.lock();
	if (event_loop) {
		event_loop->Modify(shared_from_this());
	} else {
		LOG_WARN("event modify called but event loop deleted " << this);
	}
}

std::ostream& operator<<(std::ostream &out, const EventHandler *event_handler) {
	out << "id=" << event_handler->id_ << " handle=" << event_handler->handle_;
	return out;
}


EventHandler::EventHandler()  : id_(id_counter.fetch_add(1)) {
	LOG_TRACE("event handler is created " << this);
}

EventHandler::EventHandler(Handle handle) : id_(id_counter.fetch_add(1)), handle_(handle) {
	LOG_TRACE("event handler is created " << this);
}

EventHandler::~EventHandler() {
	LOG_TRACE("event handler is destroyed " << this);
	if (event_) {
		LOG_TRACE("event handler is destroyed - calling event close " << this);
		event_->Close();
	} else if (handle_) {
		LOG_TRACE("event handler is destroyed - event never attached closing descriptor " << this);
		handle_.Close();
	}
}

void EventHandler::ReadyEvent(Events events) {
	if (event_) {
		LOG_TRACE("event handler ready " << this << " events=" << events);
		event_->Ready(events);
	}
}

void EventHandler::CloseEvent() {
	if (event_) {
		LOG_TRACE("event handler close " << this);
		event_->Close();
	}
}

void EventHandler::ModifyEvent() {
	if (event_) {
		LOG_TRACE("event handler modify " << this);
		event_->Modify();
	}
}

}
