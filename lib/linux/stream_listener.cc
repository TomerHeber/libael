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

#include <arpa/inet.h>

namespace ael {

StreamListener::StreamListener(std::shared_ptr<NewConnectionHandler> new_connection_handler, int domain, const sockaddr *addr, socklen_t addr_size) :
		new_connection_handler_(new_connection_handler),
		domain_(domain),
		addr_(std::make_unique<std::uint8_t[]>(addr_size)),
		addr_size_(addr_size)
{
	memcpy(addr_.get(), addr, addr_size);
}

std::shared_ptr<StreamListener> StreamListener::Create(std::shared_ptr<NewConnectionHandler> new_connection_handler, const std::string &ip_addr, in_port_t port) {
	LOG_INFO("creating a stream listener ip_addr=" << ip_addr << " port=" << port);

	// Try IPv4.
	sockaddr_in in4 = {};
	if (inet_pton(AF_INET, ip_addr.c_str(), &in4.sin_addr) == 1) {
		in4.sin_family = AF_INET;
		in4.sin_port = htons(port);

		return std::shared_ptr<StreamListener>(new StreamListener(new_connection_handler, AF_INET, reinterpret_cast<sockaddr*>(&in4), sizeof(in4)));
	}

	LOG_DEBUG("inet_pton for IPv6 failed ip_addr=" << ip_addr << " errno=" << errno);

	// Try IPv6.
	sockaddr_in6 in6 = {};
	if (inet_pton(AF_INET6, ip_addr.c_str(), &in6.sin6_addr) == 1) {
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons(port);

		return std::shared_ptr<StreamListener>(new StreamListener(new_connection_handler, AF_INET6, reinterpret_cast<sockaddr*>(&in6), sizeof(in6)));
	}

	LOG_DEBUG("inet_pton for IPv6 failed ip_addr=" << ip_addr << " errno=" << errno);

	throw "invalid host - inet_pton failed for both IPv4 and IPv6";
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

		auto flags = fcntl(new_fd, F_GETFL, 0);
		if (flags < 0) {
			throw std::system_error(errno, std::system_category(), "fcntl get failed");
		}

		if (fcntl(new_fd, F_SETFL, flags | O_NONBLOCK | SOCK_CLOEXEC) != 0) {
			throw std::system_error(errno, std::system_category(), "fcntl set failed");
		}

		LOG_DEBUG("listener accepted new connection " << "fd=" << event->GetFD() << " new_fd=" << new_fd);

		auto new_connection_handler = new_connection_handler_.lock();
		if (new_connection_handler) {
			new_connection_handler->HandleNewConnection(new_fd);
		} else {
			LOG_WARN("unable to handle new connections - new connection handler has been destroyed!");
		}
	}

	LOG_DEBUG("listener reached starvation limit " << "fd=" << event->GetFD());

	event->Ready();
}


}
