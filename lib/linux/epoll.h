/*
 * async_io_epoll.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_LINUX_EPOLL_H_
#define LIB_LINUX_EPOLL_H_

#include "async_io.h"

#include <unordered_map>
#include <mutex>
#include <vector>
#include <functional>

namespace ael {

class EPoll : public AsyncIO {
public:
	EPoll();
	virtual ~EPoll();

private:
	struct ReadyEvent {
		std::uint32_t flags;
		std::shared_ptr<Event> event;
	};

	virtual void Add(std::shared_ptr<Event> event);
	virtual void Modify(std::shared_ptr<Event> event);
	virtual void Remove(std::shared_ptr<Event> event);
	virtual void Ready(std::shared_ptr<Event> event, int flags);
	virtual void Wakeup();
	virtual void Process();

	void AddFinalize(std::shared_ptr<Event> event);
	void ReadyFinalize(ReadyEvent ready_event);
	void RemoveFinalize(std::shared_ptr<Event> event);

	template<typename T>
	void AddElement(T element, std::vector<T> &elements_container);
	template<typename T>
	void HandleElements(std::vector<T> &elements_container, std::function<void(EPoll*, T)> finalize_function);

	int epoll_fd_;
	int pending_fd_;
	std::mutex lock_;
	std::vector<std::shared_ptr<Event>> events_pending_add_;
	std::vector<std::shared_ptr<Event>> events_pending_remove_;
	std::vector<ReadyEvent> events_pending_ready_;
	std::unordered_map<int, std::shared_ptr<Event>> events_;
};

}

#endif /* LIB_LINUX_EPOLL_H_ */
