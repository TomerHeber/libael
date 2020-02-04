/*
 * eventloop.cc
 *
 *  Created on: Jan 31, 2020
 *      Author: tomer
 */

#include <unordered_set>

#include "eventloop.h"

namespace ael {

std::mutex table_lock;
std::unordered_set<std::shared_ptr<EventLoop>> table;

EventLoop::EventLoop() : async_io_(AsyncIO::Create()), stop_(false) {}

EventLoop::~EventLoop() {}

std::shared_ptr<EventLoop> EventLoop::Create() {
	std::shared_ptr<EventLoop> event_loop(new EventLoop);

	table_lock.lock();
	table.insert(event_loop);
	table_lock.unlock();

	event_loop->thread_ = std::make_unique<std::thread>(&EventLoop::Run, event_loop);

	return event_loop;
}

void EventLoop::Run() {
	while (!stop_) {
		async_io_->Process();
	}
}

void EventLoop::Remove(unsigned long long id) {
	lock_.lock();

	auto event_iterator = events_.find(id);
	if (event_iterator == events_.end()) {
		throw "event not found";
	}

	auto event = event_iterator->second;

	events_.erase(event_iterator);

	lock_.unlock();

	async_io_->Remove(event);
}

std::shared_ptr<Event> EventLoop::CreateStreamSocketEvent(std::shared_ptr<EventHandler> event_handler, int fd) {
	return CreateEvent(event_handler, fd, READ_FLAG | WRITE_FLAG | STREAM_FLAG);
}

std::shared_ptr<Event> EventLoop::CreateEvent(std::shared_ptr<EventHandler> event_handler, int fd, int flags) {
	auto event = std::make_shared<Event>(this, event_handler, fd, flags);

	lock_.lock();
	events_[event->GetID()] = event;
	lock_.unlock();

	async_io_->Add(event);

	return event;
}

}
