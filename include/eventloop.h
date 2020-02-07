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
#include <cstdint>
#include <atomic>

#include "event.h"

namespace ael {

class EventLoop : public std::enable_shared_from_this<EventLoop> {
public:
	static std::shared_ptr<EventLoop> Create();

	void Attach(std::shared_ptr<EventHandler> event_handler);

	virtual ~EventLoop();

private:
	EventLoop();

	void Run();
	void Remove(std::uint64_t id);

	std::shared_ptr<Event> CreateEvent(std::shared_ptr<EventHandler> event_handler, int fd, int flags);

	std::unique_ptr<std::thread> thread_;
	std::unique_ptr<class AsyncIO> async_io_;
	std::unordered_map<std::uint64_t, std::shared_ptr<Event>> events_;
	std::mutex lock_;
	std::atomic_bool stop_;

	friend class Event;
};

}

#endif /* LIB_EVENTLOOP_H_ */
