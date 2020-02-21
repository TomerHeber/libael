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

#include "handle.h"

namespace ael {

class EventLoop;
class Event;

class Events {
public:		
	static const std::uint32_t Read = 0x1;
	static const std::uint32_t Write = 0x2;
	static const std::uint32_t Close = 0x4;
	static const std::uint32_t Error = 0x8;
	static const std::uint32_t Stream = 0x16;
	
	Events() : events_(0) {}
	Events(std::uint32_t events) : events_(events) {}
	virtual ~Events() {}	

	operator std::uint32_t() const { return events_; }

private:
	std::uint32_t events_;
};

class EventHandler {
public:
	EventHandler();
	EventHandler(Handle handle);
	virtual ~EventHandler();

	virtual void HandleEvents(Handle handle, Events events) = 0;
	virtual Events GetEvents() const = 0;

	std::uint64_t GetId() const { return id_; }

	friend std::ostream& operator<<(std::ostream &out, const EventHandler *event_handler);

protected:
	void ReadyEvent(Events events);
	void CloseEvent();
	void ModifyEvent();

private:
	std::shared_ptr<Event> event_;
	const std::uint64_t id_;
	Handle handle_;

	friend EventLoop;
	friend Event;
};

class Event : public std::enable_shared_from_this<Event> {
public:
	virtual ~Event();

	friend std::ostream& operator<<(std::ostream &out, const Event *event);

	std::uint64_t GetID() const { return id_; }
	Handle GetHandle() const { return handle_; }
	Events GetEvents() const;
	std::weak_ptr<EventHandler> GetEventHandler() const { return event_handler_; }
	std::weak_ptr<EventLoop> GetEventLoop() const { return event_loop_; }

	void Close(); // The event should be "closed".
	void Modify(); // "events" state has been modified. ***Important: this function must be called within the context of the event loop (this restriction may change if required in the future).
	void Ready(Events events);	// Notifies that the event is ready to handle the given events.

private:
	Event(std::shared_ptr<EventLoop>, std::shared_ptr<EventHandler> event_handler);

	const std::uint64_t id_;
	std::weak_ptr<EventLoop> event_loop_;
	std::weak_ptr<EventHandler> event_handler_;
	Handle handle_;
	std::once_flag close_flag_;

	friend EventLoop;
};

}

#endif /* LIB_EVENT_H_ */
