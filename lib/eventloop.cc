/*
 * eventloop.cc
 *
 *  Created on: Jan 31, 2020
 *      Author: tomer
 */

#include <unordered_set>

#include "eventloop.h"
#include "log.h"
#include "async_io.h"

namespace ael {

EventLoop::EventLoop() : async_io_(AsyncIO::Create()), stop_(false) {
	LOG_TRACE("event loop is being created");
}

EventLoop::~EventLoop() {
	LOG_TRACE("event loop is destroyed");
	stop_ = true;
	async_io_->Wakeup(); // Wakeup for the loop to detect stop.
	thread_->join();
	LOG_TRACE("event loop is destroyed - thread join complete");
}

std::shared_ptr<EventLoop> EventLoop::Create() {
	std::shared_ptr<EventLoop> event_loop(new EventLoop);

	LOG_TRACE("event loop is being created - starting thread");
	event_loop->thread_ = std::make_unique<std::thread>(&EventLoop::Run, event_loop.get());

	return event_loop;
}

void EventLoop::Run() {
	LOG_TRACE("event loop thread started");

	while (!stop_) {
		async_io_->Process();
	}

	LOG_TRACE("event loop stop detected");

	lock_.lock();
	for (auto it : events_) {
		it.second->Close();
	}
	lock_.unlock();

	async_io_->Wakeup(); // Wakeup again in case there is nothing to process.

	async_io_->Process();

	LOG_TRACE("event loop thread finished");
}

void EventLoop::Remove(std::uint64_t id) {
	LOG_DEBUG("removing event id=" << id);

	lock_.lock();

	auto event_iterator = events_.find(id);
	if (event_iterator == events_.end()) {
		throw "event not found";
	}

	auto event = event_iterator->second;

	events_.erase(event_iterator);

	lock_.unlock();

	LOG_TRACE("removing event - event removed proceed to async_io remove id=" << id);

	async_io_->Remove(event);
}

std::shared_ptr<Event> EventLoop::CreateEvent(std::shared_ptr<EventHandler> event_handler, int fd, int flags) {
	std::shared_ptr<Event> event(new Event(shared_from_this(), event_handler, fd, flags));

	LOG_DEBUG("creating and adding an event id=" << event->GetID() << " fd=" << event->GetFD());

	lock_.lock();
	events_[event->GetID()] = event;
	lock_.unlock();

	LOG_TRACE("creating and adding an event - event added id=" << event->GetID() << " fd=" << event->GetFD());

	return event;
}

void EventLoop::Attach(std::shared_ptr<EventHandler> event_handler) {
	if (event_handler->event_) {
		throw "event handler already attached";
	}

	event_handler->event_ = CreateEvent(event_handler, event_handler->GetFD(), event_handler->GetFlags());

	LOG_DEBUG("event handler attaching to event loop event_id=" << event_handler->event_->GetID() << " event_fd=" << event_handler->event_->GetFD());

	async_io_->Add(event_handler->event_);

	LOG_TRACE("event handler attaching to event loop - event handler attached event_id=" << event_handler->event_->GetID() << " event_fd=" << event_handler->event_->GetFD());
}

void EventLoop::AttachInternal(std::shared_ptr<EventHandler> event_handler) {
	lock_.lock();
	internal_event_handlers_.insert(event_handler);
	lock_.unlock();
	Attach(event_handler);
}

void EventLoop::RemoveInternal(std::shared_ptr<EventHandler> event_handler) {
	std::lock_guard<std::mutex> guard(lock_);
	if (internal_event_handlers_.erase(event_handler) != 1) {
		throw "event handler found";
	}
}

}
