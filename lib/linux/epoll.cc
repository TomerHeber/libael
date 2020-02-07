/*
 * async_io.cpp
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include "epoll.h"
#include "log.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <unistd.h>
#include <errno.h>
#include <system_error>


#define MAX_EVENTS 32

namespace ael {

std::unique_ptr<AsyncIO> AsyncIO::Create() {
	return std::make_unique<EPoll>();
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

	epoll_event pending_event;
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

			AddEvents();
			RemoveEvents();

			LOG_TRACE("epoll handling pending fds - complete epoll_fd_=" << epoll_fd_);

			continue;
		}

		auto event_iterator = events_.find(event_fd);
		if (event_iterator == events_.end()) {
			LOG_DEBUG("an fd was not found in the events table - skipping epoll_fd_=" << epoll_fd_ << " event_fd=" << event_fd);
			continue;
		}

		auto event = event_iterator->second;
		auto event_handler = event->GetEventHandler();
		if (event_handler) {
			LOG_TRACE("epoll events for event - epoll_fd_=" << epoll_fd_ << " event_e.events=" << event_e.events << " event_fd=" << event_fd);
			event_handler->Handle(event, event_e.events);
		}
	}
}

void EPoll::AddEvents() {
	// This runs in the context of the EventLoop thread.

	std::vector<std::shared_ptr<Event>> events;
	lock_.lock();
	events_pending_add_.swap(events);
	lock_.unlock();

	LOG_TRACE("epoll adding events - epoll_fd_=" << epoll_fd_ << " events.size()=" << events.size());

	for (auto event : events) {
		AddFinalize(event);
	}
}

void EPoll::RemoveEvents() {
	// This runs in the context of the EventLoop thread.

	std::vector<std::shared_ptr<Event>> events;
	lock_.lock();
	events_pending_remove_.swap(events);
	lock_.unlock();

	LOG_TRACE("epoll removing events - epoll_fd_=" << epoll_fd_ << " events.size()=" << events.size());

	for (auto event : events) {
		RemoveFinalize(event);
	}
}

void EPoll::Add(std::shared_ptr<Event> event) {
	AddOrRemoveHelper(event, events_pending_add_);
}

void EPoll::Remove(std::shared_ptr<Event> event) {
	AddOrRemoveHelper(event, events_pending_remove_);
}

void EPoll::Wakeup() {
	if (eventfd_write(pending_fd_, 1) != 0) {
		throw std::system_error(errno, std::system_category(), "eventfd_write failed");
	}
}

void EPoll::AddOrRemoveHelper(std::shared_ptr<Event> event, std::vector<std::shared_ptr<Event>> &events_pending) {
	// Switch the event to be executed in the context of the EventLoop thread.

	lock_.lock();
	events_pending.push_back(event);
	if (events_pending.size() == 1) {
		// "write" to eventfd - used as an asnyc notification mechanism.
		if (eventfd_write(pending_fd_, 1) != 0) {
			throw std::system_error(errno, std::system_category(), "eventfd_write failed");
		}
	}
	lock_.unlock();
}

void EPoll::AddFinalize(std::shared_ptr<Event> event) {
	auto flags = event->GetFlags();

	LOG_TRACE("epoll adding event finalize epoll_fd_=" << epoll_fd_ << " fd=" << event->GetFD() << " id=" << event->GetID());

	epoll_event e_event;

	e_event.events = EPOLLET;

	if (flags & READ_FLAG) {
		e_event.events |= EPOLLIN;
	}

	if (flags & WRITE_FLAG) {
		e_event.events |= EPOLLOUT;
	}

	if (flags & STREAM_FLAG) {
		e_event.events |= EPOLLRDHUP;
	}

	e_event.data.fd = event->GetFD();

	events_[event->GetFD()] = event;

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event->GetFD(), &e_event) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_ADD - failed");
	}

	LOG_TRACE("epoll adding event finalize - complete epoll_fd_=" << epoll_fd_ << " fd=" << event->GetFD() << " id=" << event->GetID());
}

void EPoll::RemoveFinalize(std::shared_ptr<Event> event) {
	LOG_TRACE("epoll removing event finalize epoll_fd_=" << epoll_fd_ << " fd=" << event->GetFD() << " id=" << event->GetID());

	if (events_.erase(event->GetFD()) != 1) {
		throw "event not found";
	}

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event->GetFD(), NULL) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_DEL - failed");
	}

	LOG_TRACE("epoll removing event finalize - complete epoll_fd_=" << epoll_fd_ << " fd=" << event->GetFD() << " id=" << event->GetID());
}

Event::~Event() {
	LOG_TRACE("event is destroyed id=" << id_);
	close(fd_);
}

}

