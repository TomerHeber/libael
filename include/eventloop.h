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
#include <unordered_set>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <functional>

#include "event.h"
#include "log.h"

namespace ael {

class EventLoop : public std::enable_shared_from_this<EventLoop> {
private:
	template<class T>
	class ExecuteEventHandler : public EventHandler {
	public:
		ExecuteEventHandler(std::function<void()> func, std::weak_ptr<T> instance) :
			func_(func),
			instance_(instance) {
			LOG_TRACE("execute event handler is created");
		}

		virtual ~ExecuteEventHandler() {
			LOG_TRACE("execute event handler is destroyed");
		}

	private:
		virtual void Handle(std::shared_ptr<Event> event, std::uint32_t events) {
			auto instance = instance_.lock();
			if (instance) {
				func_();
			}

			event->Close();

			auto event_handler = event->GetEventHandler().lock();
			auto event_loop = event->GetEventLoop().lock();

			if (event_handler && event_loop) {
				event_loop->RemoveInternal(event_handler);
			}
		}

		std::function<void()> func_;
		std::weak_ptr<T> instance_;
	};

public:
	static std::shared_ptr<EventLoop> Create();

	void Attach(std::shared_ptr<EventHandler> event_handler);

	template<class Function, class Instance, class... Args>
	void Execute(Function func, std::shared_ptr<Instance> instance, Args&&... args) {
		auto execute_event_handler = std::make_shared<ExecuteEventHandler<Instance>>(std::bind(func, instance.get(), std::forward<Args>(args)...), instance);
		AttachInternal(execute_event_handler);
	}

	virtual ~EventLoop();

private:
	EventLoop();

	void Run();
	void Remove(std::uint64_t id);

	void AttachInternal(std::shared_ptr<EventHandler> event_handler);
	void RemoveInternal(std::shared_ptr<EventHandler> event_handler);

	std::shared_ptr<Event> CreateEvent(std::shared_ptr<EventHandler> event_handler, int fd, int flags);

	std::unique_ptr<std::thread> thread_;
	std::unique_ptr<class AsyncIO> async_io_;
	std::unordered_map<std::uint64_t, std::shared_ptr<Event>> events_;
	std::unordered_set<std::shared_ptr<EventHandler>> internal_event_handlers_;
	std::mutex lock_;
	std::atomic_bool stop_;

	friend Event;
};

}

#endif /* LIB_EVENTLOOP_H_ */
