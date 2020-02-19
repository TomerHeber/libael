/*
 * eventloop.cc
 *
 *  Created on: Jan 31, 2020
 *      Author: tomer
 */

#include <unordered_set>

#include "log.h"
#include "async_io.h"
#include "event_loop.h"

namespace ael {

static std::mutex table_lock;
static std::unordered_set<std::shared_ptr<EventLoop>> table;

void EventLoop::DestroyAll() {
	std::unordered_set<std::shared_ptr<EventLoop>> table_swap;

	table_lock.lock();
	table_swap.swap(table);
	table_lock.unlock();

	for (auto event_loop : table_swap) {
		event_loop->Stop();
	}
	table_swap.clear();
}

EventLoop::EventLoop() : async_io_(AsyncIO::Create()), stop_(false) {
	LOG_TRACE("event loop is being created");
}

EventLoop::~EventLoop() {
	LOG_TRACE("event loop is destroyed");
}

std::shared_ptr<EventLoop> EventLoop::Create() {
	std::shared_ptr<EventLoop> event_loop(new EventLoop);

	table_lock.lock();
	table.insert(event_loop);
	table_lock.unlock();

	LOG_TRACE("event loop is being created - starting thread");
	event_loop->thread_ = std::make_unique<std::thread>(&EventLoop::Run, event_loop.get());

	return event_loop;
}

void EventLoop::Stop() {
	LOG_TRACE("event loop is stopping");
	stop_ = true;
	async_io_->Wakeup(); // Wakeup for the loop to detect stop.
	thread_->join();
	LOG_TRACE("event loop stopped");
}

void EventLoop::Run() {
	LOG_DEBUG("event loop thread started");

	while (!stop_) {
		async_io_->Process();
	}

	LOG_DEBUG("event loop stop detected");

	lock_.lock();
	for (auto it : events_) {
		it.second->Close();
	}
	lock_.unlock();

	async_io_->Wakeup(); // Wakeup again in case there is nothing to process.

	async_io_->Process();

	LOG_DEBUG("event loop thread finished");
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

void EventLoop::Ready(std::shared_ptr<Event> event, int flags) {
	auto event_id = event->GetID();

	LOG_TRACE("readying an event id=" << event_id << " flags=" << flags);

	async_io_->Ready(event, flags);
}

void EventLoop::Modify(std::shared_ptr<Event> event) {
	if (thread_->get_id() != std::this_thread::get_id()) {
		throw "Modify() called outside the scope of the event loop";
	}

	async_io_->Modify(event);
}

std::shared_ptr<Event> EventLoop::CreateEvent(std::shared_ptr<EventHandler> event_handler) {
	std::shared_ptr<Event> event(new Event(shared_from_this(), event_handler));

	LOG_TRACE("creating and adding an event id=" << event->GetID() << " fd=" << event->GetFD());

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

	event_handler->event_= CreateEvent(event_handler);

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

EventLoop::ExecuteHandler::ExecuteHandler(std::function<void()> func, std::weak_ptr<void> instance) :
		func_(func),
		instance_(instance) {
	LOG_TRACE("execute handler is created");
}

EventLoop::ExecuteHandler::~ExecuteHandler() {
	LOG_TRACE("execute handler is destroyed");
}

void EventLoop::ExecuteHandler::Handle(std::uint32_t events) {
	std::ignore = events;

	auto instance = instance_.lock();
	if (instance) {
		func_();
	}

	event_->Close();

	auto event_handler = event_->GetEventHandler().lock();
	auto event_loop = event_->GetEventLoop().lock();

	if (event_handler && event_loop) {
		event_loop->RemoveInternal(event_handler);
	}
}

EventLoop::TimerHandler::TimerHandler(int fd, std::function<void()> func, std::weak_ptr<void> instance) :
		EventHandler(fd), func_(func), instance_(instance) {
	LOG_TRACE("timer handler is created");
}

EventLoop::TimerHandler::~TimerHandler() {
	LOG_TRACE("timer handler is destroyed");
}

}
