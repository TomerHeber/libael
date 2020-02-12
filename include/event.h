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

class EventLoop;
class Event;

class EventHandler {
public:
	EventHandler(int fd = -1);
	virtual ~EventHandler();

	virtual void Handle(std::uint32_t events) = 0;
	virtual int GetFlags() const = 0;

	int GetFD() const;

protected:
	std::shared_ptr<Event> event_;

private:
	int fd_;

	friend EventLoop;
};

class Event : public std::enable_shared_from_this<Event> {
public:
	virtual ~Event();

	std::uint64_t GetID() const { return id_; }
	int GetFD() const { return fd_; }
	int GetFlags() const;
	std::weak_ptr<EventHandler> GetEventHandler() const { return event_handler_; }
	std::weak_ptr<EventLoop> GetEventLoop() const { return event_loop_; }

	void Close(); // The event should be "closed".
	void Modify(); // "events" state has been modified. ***Important: this function must be called within the context of the event loop (this restriction may change if required in the future).
	void Ready(int flags);	// Notifies that the event is ready to handle events (based on flags).

private:
	Event(std::shared_ptr<EventLoop>, std::shared_ptr<EventHandler> event_handler);

	std::uint64_t id_;
	std::weak_ptr<EventLoop> event_loop_;
	std::weak_ptr<EventHandler> event_handler_;
	int fd_;
	std::once_flag close_flag_;

	friend EventLoop;
};

}

#endif /* LIB_EVENT_H_ */
