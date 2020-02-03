/*
 * eventloop.cc
 *
 *  Created on: Jan 31, 2020
 *      Author: tomer
 */

#include <mutex>
#include <unordered_set>


#include "eventloop.h"

namespace ael {

std::mutex table_lock;
std::unordered_set<std::shared_ptr<EventLoop>> table;

EventLoop::EventLoop() : stop_(false) {

}

EventLoop::~EventLoop() {
	// TODO Auto-generated destructor stub
}

std::shared_ptr<EventLoop> EventLoop::Create() {
	std::shared_ptr<EventLoop> event_loop(new EventLoop);

	table_lock.lock();
	table.insert(event_loop);
	table_lock.unlock();

	event_loop->thread_ = std::unique_ptr<std::thread>(new std::thread(&EventLoop::Run, event_loop));
	event_loop->async_io_ = AsyncIO::Create();

	return event_loop;
}

void EventLoop::Run() {
	while (!stop_) {

	}
}

}
