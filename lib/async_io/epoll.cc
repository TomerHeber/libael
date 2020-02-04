/*
 * async_io.cpp
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <unistd.h>
#include <errno.h>

#include <system_error>

#include "epoll.h"

#define MAX_EVENTS 10

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
}

EPoll::~EPoll() {
	close(pending_fd_);
	close(epoll_fd_);
}

void EPoll::Process() {
	epoll_event events[MAX_EVENTS];

	auto nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
	if (nfds == -1) {
		throw std::system_error(errno, std::system_category(), "epoll_wait failed");
	}

	for (auto n = 0; n < nfds; n++) {
		auto event = events[n];
		auto event_fd = event.data.fd;

		if (event_fd == pending_fd_) {
			AddEvents();
			RemoveEvents();
			continue;
		}
	}
}

void EPoll::AddEvents() {
	//TODO
}

void EPoll::RemoveEvents() {
	//TODO
}

void EPoll::Add(std::shared_ptr<Event> event) {
	//TODO
}

void EPoll::Remove(std::shared_ptr<Event> event) {
	//TODO
}

}
/*
void EPoll::Close(std::shared_ptr<Event> event) {
	lock_.lock();

	auto context_iterator = contexts_.find(event->GetFD());
	if (context_iterator == contexts_.end()) {
		throw "epoll context not found";
	}

	auto context = context_iterator->second;

	contexts_.erase(event->GetFD());

	lock_.unlock();

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event->GetFD(), NULL) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_DEL - failed");
	}

	context->Release();
}

void EPoll::Add(std::shared_ptr<Event> event, int flags) {
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

	auto context = new Context(event);

	lock_.lock();
	contexts_[event->GetFD()] = context;
	lock_.unlock();

	e_event.data.ptr = context;

	if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event->GetFD(), &e_event) != 0) {
		throw std::system_error(errno, std::system_category(), "epoll_ctl - EPOLL_CTL_ADD - failed");
	}
}
*/
