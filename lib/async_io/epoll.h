/*
 * async_io_epoll.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_ASYNC_IO_EPOLL_H_
#define LIB_ASYNC_IO_EPOLL_H_

#include "async_io.h"

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>

namespace ael {

class EPoll : public AsyncIO {
public:
	EPoll();
	virtual ~EPoll();

private:
	virtual void Process();
	virtual void Add(std::shared_ptr<Event> event);
	virtual void Remove(std::shared_ptr<Event> event);

	void AddEvents();
	void RemoveEvents();

	int epoll_fd_;
	int pending_fd_;
	std::mutex lock_;
	std::vector<std::shared_ptr<Event>> events_pending_close;
	std::vector<std::shared_ptr<Event>> events_pending_add;
};

}

#endif /* LIB_ASYNC_IO_EPOLL_H_ */
