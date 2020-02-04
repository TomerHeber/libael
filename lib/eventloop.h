/*
 * eventloop.h
 *
 *  Created on: Jan 31, 2020
 *      Author: tomer
 */

#ifndef LIB_EVENTLOOP_H_
#define LIB_EVENTLOOP_H_

#include <thread>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "async_io.h"
#include "event.h"

namespace ael {

class EventLoop {
public:
	static std::shared_ptr<EventLoop> Create();

	std::shared_ptr<Event> CreateStreamSocketEvent(std::shared_ptr<EventHandler> event_handler, int fd);

	virtual ~EventLoop();

private:
	EventLoop();

	void Run();
	void Remove(unsigned long long id);

	std::shared_ptr<Event> CreateEvent(std::shared_ptr<EventHandler> event_handler, int fd, int flags);

	std::unique_ptr<std::thread> thread_;
	std::unique_ptr<AsyncIO> async_io_;
	std::unordered_map<unsigned long long, std::shared_ptr<Event>> events_;
	std::mutex lock_;
	bool stop_;

	friend class Event;
};

}

#endif /* LIB_EVENTLOOP_H_ */
