/*
 * stream_listener_linux.cc
 *
 *  Created on: Feb 20, 2020
 *      Author: tomer
 */

#include "stream_listener.h"
#include "config.h"
#include "log.h"
#include "async_io.h"

#include <cstdint>

#include <sys/epoll.h>

#include <unistd.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/socket.h>

namespace ael {

void StreamListener::HandleEvents(Handle handle, std::uint32_t events) {
	if (!(events & EPOLLIN)) {
		LOG_WARN("received an non EPOLLIN event for a listener " << this << " events=" << events);
		return;
	}

	// To avoid starvation limit the number of "accepts".

	for (auto i = 0; i < GLOBAL_CONFIG.listen_starvation_limit_; i++) {
		LOG_TRACE("listener about to call accept " << this << " i=" << i)

		auto new_fd = accept4(handle, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);

		if (new_fd < 0) {
			switch (errno) {
			case EAGAIN:
				LOG_DEBUG("listener nothing to accept " << this)
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
				LOG_DEBUG("listener accept failed " << this << " errno=" << errno);
				continue;
			}
		}

		LOG_DEBUG("listener accepted new connection " << this << " new_fd=" << new_fd);

		auto new_connection_handler = new_connection_handler_.lock();
		if (new_connection_handler) {
			new_connection_handler->HandleNewConnection(new_fd);
		} else {
			LOG_WARN("unable to handle new connections - new connection handler has been destroyed " << this);
		}
	}

	LOG_DEBUG("listener reached starvation limit " << this);

	ReadyEvent(READ_FLAG);
}

}

