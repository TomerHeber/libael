/*
 * async_io.cpp
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include "config.h"
#include "epoll.h"
#include "log.h"

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cerrno>
#include <system_error>

#define MAX_EVENTS 32

namespace ael {

std::unique_ptr<AsyncIO> AsyncIO::Create() {
#ifdef HAVE_SYS_EPOLL_H
	return std::make_unique<EPoll>();
#endif
}

EPoll::EPoll() {
	epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd_ < 0) {
		throw std::system_error(errno, std::system_category(), "epoll_create1 failed");
	}

	pending_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (pending_fd_ < 0) {
		throw std::system_error(errno, std::system_category(), "eventfd failed");
	}

	epoll_event pending_event = {};
	pending_event.events = EPOLLET | EPOLLIN;
	pending_event.data.fd = pending_fd_;

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pending_fd_, &pending_event) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_ADD - failed");
	}

	LOG_TRACE("epoll is created epoll_fd_=" << epoll_fd_ << " pending_fd_" << pending_fd_);
}

EPoll::~EPoll() {
	LOG_TRACE("epoll is destroyed epoll_fd_=" << epoll_fd_ << " pending_fd_" << pending_fd_);
	close(pending_fd_);
	close(epoll_fd_);
}

static std::uint32_t GetEPollEvents(int flags) {
	std::uint32_t events = 0;

	if (flags & CLOSE_FLAG) {
		events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
	} else {
		if (flags & READ_FLAG) {
			events |= EPOLLIN;
		}

		if (flags & WRITE_FLAG) {
			events |= EPOLLOUT;
		}

		if (flags & STREAM_FLAG) {
			events |= EPOLLRDHUP;
		}
	}

	return events;
}

template<typename T>
void EPoll::HandleElements(std::vector<T> &elements_container, std::function<void(EPoll*, T)> finalize_function) {
	// This runs in the context of the EventLoop thread.
	std::vector<T> elements_container_swap;
	lock_.lock();
	elements_container.swap(elements_container_swap);
	lock_.unlock();

	for (T element : elements_container_swap) {
		finalize_function(this, element);
	}
}

template<typename T>
void EPoll::AddElement(T element, std::vector<T> &elements_container) {
	// Add the element to be used in the context of the EventLoop thread.
	lock_.lock();
	elements_container.push_back(element);
	if (elements_container.size() == 1) {
		Wakeup();
	}
	lock_.unlock();
}

void EPoll::Process() {
	epoll_event events[MAX_EVENTS];

	auto nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
	if (nfds == -1) {
		throw std::system_error(errno, std::system_category(), "epoll_wait failed");
	}

	LOG_DEBUG("epoll received events epoll_fd_=" << epoll_fd_ << " nfds=" << nfds);

	for (auto n = 0; n < nfds; n++) {
		auto event_e = events[n];
		auto event_fd = event_e.data.fd;

		if (event_fd == pending_fd_) {
			LOG_TRACE("epoll handling pending fds epoll_fd_=" << epoll_fd_);

			eventfd_t val;

			while (eventfd_read(pending_fd_, &val) == 0); // Since EPOLLET is set keep reading until the counter is zeroed (EAGAIN is received).

			HandleElements<std::shared_ptr<Event>>(events_pending_add_, &EPoll::AddFinalize);
			HandleElements<std::shared_ptr<Event>>(events_pending_remove_, &EPoll::RemoveFinalize);
			HandleElements<ReadyEvent>(events_pending_ready_, &EPoll::ReadyFinalize);

			LOG_TRACE("epoll handling pending fds - complete epoll_fd_=" << epoll_fd_);

			continue;
		}

		auto event_iterator = events_.find(event_fd);
		if (event_iterator == events_.end()) {
			LOG_DEBUG("an fd was not found in the events table - skipping epoll_fd_=" << epoll_fd_ << " event_fd=" << event_fd);
			continue;
		}

		auto event = event_iterator->second;
		auto event_handler = event->GetEventHandler().lock();
		if (event_handler) {
			LOG_TRACE("epoll events for event epoll_fd_=" << epoll_fd_ << " event_e.events=" << event_e.events << " event_fd=" << event_fd);
			event_handler->HandleEvents(event_fd, event_e.events);
		} else {
			LOG_TRACE("epoll events for event - event handler destroyed epoll_fd_=" << epoll_fd_ << " event_e.events=" << event_e.events << " event_fd=" << event_fd);
		}
	}
}

void EPoll::Add(std::shared_ptr<Event> event) {
	AddElement(event, events_pending_add_);
}

void EPoll::Modify(std::shared_ptr<Event> event) {
	auto handle = event->GetHandle();
	auto flags = event->GetFlags();

	LOG_TRACE("epoll modifying event mode epoll_fd_=" << epoll_fd_ << " handle=" << handle << " flags=" << flags << " id=" << event->GetID());

	epoll_event e_event = {};
	e_event.data.fd = handle;
	e_event.events = GetEPollEvents(flags) | EPOLLET;

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, handle, &e_event) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_MOD - failed");
	}
}

void EPoll::Remove(std::shared_ptr<Event> event) {
	AddElement(event, events_pending_remove_);
}

void EPoll::Ready(std::shared_ptr<Event> event, int flags) {
	uint32_t epoll_flags = GetEPollEvents(flags);

	ReadyEvent ready_event;
	ready_event.event = event;
	ready_event.flags = epoll_flags;

	AddElement(ready_event, events_pending_ready_);
}

void EPoll::Wakeup() {
	if (eventfd_write(pending_fd_, 1) != 0) {
		// "write" to eventfd - used as an asnyc notification mechanism.
		throw std::system_error(errno, std::system_category(), "eventfd_write failed");
	}
}

void EPoll::AddFinalize(std::shared_ptr<Event> event) {
	auto flags = event->GetFlags();
	auto handle = event->GetHandle();

	LOG_TRACE("epoll adding event finalize epoll_fd_=" << epoll_fd_ << " handle=" << handle << " flags=" << flags << " id=" << event->GetID());

	if (!handle) {
		// No descriptor. Just call handle and exit.
		auto event_handler = event->GetEventHandler().lock();
		if (event_handler) {
			LOG_TRACE("handling an event with no fd epoll_fd_=" << epoll_fd_ << " id=" << event->GetID());
			event_handler->HandleEvents(handle, 0);
		}
		return;
	}

	epoll_event e_event = {};


	e_event.events = EPOLLET | GetEPollEvents(flags);
	e_event.data.fd = handle;

	events_[handle] = event;

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, handle, &e_event) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_ADD - failed");
	}

	LOG_TRACE("epoll adding event finalize - complete epoll_fd_=" << epoll_fd_ << " handle=" << handle << " id=" << event->GetID());
}

void EPoll::RemoveFinalize(std::shared_ptr<Event> event) {
	auto handle = event->GetHandle();

	LOG_TRACE("epoll removing event finalize epoll_fd_=" << epoll_fd_ << " handle=" << handle << " id=" << event->GetID());

	if (!handle) {
		// No descriptor. Just exit.
		return;
	}

	if (events_.erase(handle) != 1) {
		throw "event not found";
	}

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, handle, nullptr) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_DEL - failed");
	}

	LOG_TRACE("epoll removing event finalize - complete epoll_fd_=" << epoll_fd_ << " handle=" << handle << " id=" <<  event->GetID());
}

void EPoll::ReadyFinalize(ReadyEvent ready_event) {
	auto handle = ready_event.event->GetHandle();
	auto id = ready_event.event->GetID();

	LOG_TRACE("epoll ready event finalize epoll_fd_=" << epoll_fd_ << " handle=" << handle << " id=" << id << " flags=" << ready_event.flags);

	auto event_iterator = events_.find(handle);
	if (event_iterator == events_.end() || event_iterator->second->GetID() != id) {
		LOG_TRACE("epoll ready event finalize - event no longer registered (ignore) epoll_fd_=" << epoll_fd_ << " handle=" << handle << " id=" << id << " flags=" << ready_event.flags);
		return;
	}

	auto event_handler = ready_event.event->GetEventHandler().lock();
	if (event_handler) {
		event_handler->HandleEvents(handle, ready_event.flags);
	} else {
		LOG_TRACE("epoll ready event finalize - event_handler destroyed epoll_fd_=" << epoll_fd_ << " handle=" << handle << " id=" << id << " flags=" << ready_event.flags);
	}
}

}

