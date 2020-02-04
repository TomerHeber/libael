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

namespace ael {

class EventHandler {

};

class Event {
public:
	Event(class EventLoop *event_loop, std::weak_ptr<EventHandler> event_handler, int fd, int flags);
	virtual ~Event();

	unsigned long long GetID() const { return id_; }
	int GetFD() const { return fd_; }
	int GetFlags() const { return flags_; }

	void Close();

private:
	unsigned long long id_;
	class EventLoop *event_loop_;
	std::weak_ptr<EventHandler> event_handler_;
	int fd_;
	int flags_;
	std::once_flag close_flag_;
};

}

#endif /* LIB_EVENT_H_ */
