/*
 * stream_listener.cc
 *
 *  Created on: Feb 5, 2020
 *      Author: tomer
 */

#include "linux/stream_listener.h"

#include "async_io.h"
#include "config.h"
#include "log.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/epoll.h>

namespace ael {

StreamListener::StreamListener(int domain, const sockaddr *addr, socklen_t addr_size) :
		domain_(domain),
		addr_(std::make_unique<std::uint8_t[]>(addr_size)),
		addr_size_(addr_size)
{
	memcpy(addr_.get(), addr, addr_size);
}

int StreamListener::GetFlags() const {
	return READ_FLAG;
}

int StreamListener::GetFD() const {
	LOG_TRACE("listener GetFD is called")

	auto fd = socket(domain_, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "socket failed");
	}

	if (bind(fd, reinterpret_cast<sockaddr*>(addr_.get()), addr_size_) != 0) {
		throw std::system_error(errno, std::system_category(), "bind failed");
	}

	if (listen(fd, GLOBAL_CONFIG.listen_backlog_) != 0) {
		throw std::system_error(errno, std::system_category(), "listen failed");
	}

	LOG_TRACE("created descriptor for listener " << "fd=" << fd);

	return fd;
}

void StreamListener::Handle(std::shared_ptr<Event> event, std::uint32_t events) {
	if (!(events & EPOLLIN)) {
		LOG_WARN("received an non EPOLLIN event for a listener " << "fd=" << event->GetFD() << " events=" << events);
		return;
	}

	// To avoid starvation limit the number of "accepts".

	for (auto i = 0; i < GLOBAL_CONFIG.listen_starvation_limit_; i++) {
		LOG_TRACE("listener about to call accept " << "fd=" << event->GetFD() << "i=" << i)

		auto new_fd = accept(event->GetFD(), NULL, 0);

		if (new_fd < 0) {
			switch (errno) {
			case EAGAIN:
				LOG_DEBUG("listener nothing to accept " << "fd=" << event->GetFD())
				return;
			case EBADF:
			case EFAULT:
			case EINVAL:
			case EMFILE:
			case ENFILE:
			case ENOBUFS:
			case ENOMEM:
			case ENOTSOCK:
				throw std::system_error(errno, std::system_category(), "accept failed");
			default:
				LOG_DEBUG("listener accept failed " << "fd=" << event->GetFD() << " errno=" << errno);
				continue;
			}
		}

		int flags = fcntl(new_fd, F_GETFL, 0);
		if (flags < 0) {
			throw std::system_error(errno, std::system_category(), "fcntl get failed");
		}

		if (fcntl(new_fd, F_SETFL, flags | O_NONBLOCK | SOCK_CLOEXEC) != 0) {
			throw std::system_error(errno, std::system_category(), "fcntl set failed");
		}

		LOG_DEBUG("listener accepted new connection " << "fd=" << event->GetFD() << " new_fd=" << new_fd);

		HandleNewConnection(new_fd);
	}

	LOG_DEBUG("listener reached starvation limit " << "fd=" << event->GetFD());

	event->Ready();
}


}
