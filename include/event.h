/*
 * event.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_EVENT_H_
#define LIB_EVENT_H_

#include <memory>
#include <mutex>
#include <cstdint>

namespace ael {

class NewConnectionHandler {
public:
	NewConnectionHandler() {}
	virtual ~NewConnectionHandler() {}

	virtual void HandleNewConnection(int fd) = 0;
};

class EventLoop;

class EventHandler {
public:
	EventHandler();
	virtual ~EventHandler();

	virtual void Handle(std::shared_ptr<class Event> event, std::uint32_t events) = 0;

private:
	std::shared_ptr<class Event> event_;

	virtual int GetFlags() const { return 0; }
	virtual int GetFD() const { return -1; }

	friend EventLoop;
};

class Event {
public:
	virtual ~Event();

	std::uint64_t GetID() const { return id_; }
	int GetFD() const { return fd_; }
	int GetFlags() const { return flags_; }
	std::weak_ptr<EventHandler> GetEventHandler() const { return event_handler_; }
	std::weak_ptr<EventLoop> GetEventLoop() const { return event_loop_; }

	void Close();
	void Ready();

private:
	Event(std::weak_ptr<EventLoop>, std::weak_ptr<EventHandler> event_handler, int fd, int flags);

	std::uint64_t id_;
	std::weak_ptr<EventLoop> event_loop_;
	std::weak_ptr<EventHandler> event_handler_;
	int fd_;
	int flags_;
	std::once_flag close_flag_;

	friend EventLoop;
};

}

#endif /* LIB_EVENT_H_ */
