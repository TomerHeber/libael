/*
 * stream_listener.cc
 *
 *  Created on: Feb 5, 2020
 *      Author: tomer
 */

#include "config.h"
#include "stream_listener.h"
#include "async_io.h"
#include "log.h"

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

namespace ael {

std::ostream& operator<<(std::ostream &out, const StreamListener *stream_listener) {
	const EventHandler *event_handler = stream_listener;
	out << event_handler;
	return out;
}

StreamListener::StreamListener(std::shared_ptr<NewConnectionHandler> new_connection_handler, Handle handle) :
		EventHandler(handle),
		new_connection_handler_(new_connection_handler) {
}

std::shared_ptr<StreamListener> StreamListener::Create(std::shared_ptr<NewConnectionHandler> new_connection_handler, const std::string &ip_addr, std::uint16_t port) {
	LOG_INFO("creating a stream listener ip_addr=" << ip_addr << " port=" << port);

	auto handle = Handle::CreateStreamListenerHandle(ip_addr, port);
	return std::shared_ptr<StreamListener>(new StreamListener(new_connection_handler, handle));
}

int StreamListener::GetFlags() const {
	return READ_FLAG;
}

void StreamListener::HandleEvents(Handle handle, std::uint32_t events) {
	if (!(events & EPOLLIN)) {
		LOG_WARN("received an non EPOLLIN event for a listener " << this << " events=" << events);
		return;
	}

	// To avoid starvation limit the number of "accepts".

	for (auto i = 0; i < GLOBAL_CONFIG.listen_starvation_limit_; i++) {
		LOG_TRACE("listener about to call accept " << this << " i=" << i)

#ifdef HAVE_ACCEPT4
		auto new_fd = accept4(handle, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif

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
