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
#include <chrono>

#include "event.h"

namespace ael {

class Cancellable : public EventHandler {
public:
	Cancellable(Handle handle) : EventHandler(handle) {}
	virtual ~Cancellable() {}

	virtual void Cancel() = 0;
};

class EventLoop : public std::enable_shared_from_this<EventLoop> {
public:
	static std::shared_ptr<EventLoop> Create();
	static void DestroyAll();

	void Attach(std::shared_ptr<EventHandler> event_handler);

	template<class Function, class Instance, class... Args>
	void ExecuteOnce(Function func, std::shared_ptr<Instance> instance, Args&&... args) {
		auto execute_handler = std::make_shared<ExecuteHandler>(std::bind(func, instance.get(), std::forward<Args>(args)...), instance);
		AttachInternal(execute_handler);
	}

	template<class Rep, class Period,class Function, class Instance, class... Args>
	std::shared_ptr<Cancellable> ExecuteOnceIn(const std::chrono::duration<Rep, Period> &execute_in, Function func, std::shared_ptr<Instance> instance, Args&&... args) {
		auto timer_handler = TimerHandler::Create(std::chrono::nanoseconds(0), std::chrono::nanoseconds(execute_in), std::bind(func, instance.get(), std::forward<Args>(args)...), instance);
		AttachInternal(timer_handler);
		return timer_handler;
	}

	template<class Rep, class Period,class Function, class Instance, class... Args>
	std::shared_ptr<Cancellable> ExecuteInterval(const std::chrono::duration<Rep, Period> &interval, Function func, std::shared_ptr<Instance> instance, Args&&... args) {
		auto timer_handler = TimerHandler::Create(std::chrono::nanoseconds(interval), std::chrono::nanoseconds(0), std::bind(func, instance.get(), std::forward<Args>(args)...), instance);
		AttachInternal(timer_handler);
		return timer_handler;
	}

	template<class Rep1, class Period1, class Rep2, class Period2, class Function, class Instance, class... Args>
	std::shared_ptr<Cancellable> ExecuteIntervalIn(const std::chrono::duration<Rep1, Period1> &interval, const std::chrono::duration<Rep2, Period2> &execute_in, Function func, std::shared_ptr<Instance> instance, Args&&... args) {
		auto timer_handler = TimerHandler::Create(std::chrono::nanoseconds(interval), std::chrono::nanoseconds(execute_in), std::bind(func, instance.get(), std::forward<Args>(args)...), instance);
		AttachInternal(timer_handler);
		return timer_handler;
	}

	virtual ~EventLoop();

private:
	EventLoop();

	void Run();
	void Remove(std::uint64_t id);
	void Ready(std::shared_ptr<Event> event, int flags);
	void Modify(std::shared_ptr<Event> event);
	void Stop();

	void AttachInternal(std::shared_ptr<EventHandler> event_handler);
	void RemoveInternal(std::shared_ptr<EventHandler> event_handler);

	std::shared_ptr<Event> CreateEvent(std::shared_ptr<EventHandler> event_handler);

	std::unique_ptr<std::thread> thread_;
	std::unique_ptr<class AsyncIO> async_io_;
	std::unordered_map<std::uint64_t, std::shared_ptr<Event>> events_;
	std::unordered_set<std::shared_ptr<EventHandler>> internal_event_handlers_;
	std::mutex lock_;
	std::atomic_bool stop_;

	friend Event;

	class ExecuteHandler : public EventHandler {
	public:
		ExecuteHandler(std::function<void()> func, std::weak_ptr<void> instance);
		virtual ~ExecuteHandler();

	private:
		void HandleEvents(Handle handle, std::uint32_t events) override;
		int GetFlags() const override { return 0; }

		std::function<void()> func_;
		std::weak_ptr<void> instance_;
	};

	class TimerHandler : public Cancellable {
	public:
		static std::shared_ptr<Cancellable> Create(const std::chrono::nanoseconds &interval, const std::chrono::nanoseconds &execute_in, std::function<void()> func, std::weak_ptr<void> instance);
		TimerHandler(Handle handle, bool run_once, std::function<void()> func, std::weak_ptr<void> instance);

		virtual ~TimerHandler();
	private:
		void HandleEvents(Handle handle, std::uint32_t events) override;
		int GetFlags() const override;
		void Cancel() override;

		bool run_once_;
		std::function<void()> func_;
		std::weak_ptr<void> instance_;
		std::atomic_bool canceled_;
	};
};

}

#endif /* LIB_EVENTLOOP_H_ */
