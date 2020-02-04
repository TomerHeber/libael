/*
 * event.cc
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include "event.h"
#include "eventloop.h"

#include <atomic>

namespace ael {

std::atomic_ullong id_counter(0);

Event::Event(EventLoop *event_loop, std::weak_ptr<EventHandler> event_handler, int fd, int flags) :
		event_loop_(event_loop),
		event_handler_(event_handler),
		fd_(fd),
		flags_(flags) {
	id_ = id_counter.fetch_add(1);
}

void Event::Close() {
	std::call_once(close_flag_, &EventLoop::Remove, event_loop_, id_);
}

}
