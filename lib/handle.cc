/*
 * Handle.cc
 *
 *  Created on: Feb 19, 2020
 *      Author: tomer
 */

#include "config.h"
#include "handle.h"
#include "log.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIMERFD_H
#include <sys/timerfd.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

namespace ael {

std::ostream& operator<<(std::ostream &out, const Handle handle) {
	out << handle.fd_;
	return out;
}

static timespec ToTimeSpec(const std::chrono::nanoseconds &tm) {
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(tm);
	std::chrono::nanoseconds nanoseconds = tm - seconds;

	timespec ts;
	ts.tv_sec = seconds.count();
	ts.tv_nsec = nanoseconds.count();

	return ts;
}

Handle Handle::CreateTimerHandle(const std::chrono::nanoseconds &interval, const std::chrono::nanoseconds &value) {
	auto fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "timerfd_create - failed");
	}

	if (interval.count() == 0 && value.count() == 0) {
		throw "invalid interval values (both zero nanoseconds)";
	}

	LOG_TRACE("timer handler time should execute in " << value.count() << " nanoseconds and interval set to " << interval.count() << " nanoseconds");

	itimerspec its;
	its.it_interval = ToTimeSpec(interval);
	its.it_value = ToTimeSpec(value);

	if (its.it_value.tv_nsec == 0 && its.it_value.tv_sec == 0) {
		// Start as soon as possible.
		its.it_value.tv_nsec = 1;
	}

	if (timerfd_settime(fd, 0, &its, nullptr) != 0) {
		close(fd);
		throw std::system_error(errno, std::system_category(), "timerfd_settime - failed");
	}

	return Handle(fd);
}

struct SockAddr {
	sockaddr *addr;
	std::size_t addr_size;
	int domain;
	sockaddr_in in4;
	sockaddr_in6 in6;
};

static void GetSockAddr(const std::string &ip_addr, std::uint16_t port, SockAddr *sock_addr) {
	sock_addr->addr = nullptr;

	// Try IPv4.
	sock_addr->in4 = {};
	if (inet_pton(AF_INET, ip_addr.c_str(), &sock_addr->in4.sin_addr) == 1) {
		sock_addr->in4.sin_family = AF_INET;
		sock_addr->in4.sin_port = htons(port);
		sock_addr->addr = reinterpret_cast<sockaddr*>(&sock_addr->in4);
		sock_addr->addr_size = sizeof(sock_addr->in4);
		sock_addr->domain = AF_INET;
		return;
	}

	// Try IPv6.
	sock_addr->in6 = {};
	if (inet_pton(AF_INET6, ip_addr.c_str(), &sock_addr->in6.sin6_addr) == 1) {
		sock_addr->in6.sin6_family = AF_INET6;
		sock_addr->in6.sin6_port = htons(port);
		sock_addr->addr = reinterpret_cast<sockaddr*>(&sock_addr->in6);
		sock_addr->addr_size = sizeof(sock_addr->in6);
		sock_addr->domain = AF_INET6;
		return;
	}

	throw "invalid host - inet_pton failed for both IPv4 and IPv6";
}

Handle Handle::CreateStreamListenerHandle(const std::string &ip_addr, std::uint16_t port) {
	SockAddr sock_addr;

	GetSockAddr(ip_addr, port, &sock_addr);

	auto fd = socket(sock_addr.domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "socket failed");
	}

	if (bind(fd, sock_addr.addr, sock_addr.addr_size) != 0) {
		throw std::system_error(errno, std::system_category(), "bind failed");
	}

	if (listen(fd, GLOBAL_CONFIG.listen_backlog_) != 0) {
		throw std::system_error(errno, std::system_category(), "listen failed");
	}

	LOG_TRACE("created descriptor for listener " << "fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);

	return fd;
}

Handle Handle::CreateStreamHandle(const std::string &ip_addr, std::uint16_t port, bool &is_connected) {
	SockAddr sock_addr;

	GetSockAddr(ip_addr, port, &sock_addr);

	auto fd = socket(sock_addr.domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "socket failed");
	}

	LOG_TRACE("creating stream buffer - connecting fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);

	auto connect_ret = connect(fd, sock_addr.addr, sock_addr.addr_size);
	if (connect_ret == 0) {
		LOG_TRACE("creating stream buffer - connected fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
		is_connected = true;
	} else {
		is_connected = false;
		switch(errno) {
		case EINPROGRESS:
			LOG_TRACE("creating stream buffer - connecting in progress fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
			break;
		case EAFNOSUPPORT:
		case EALREADY:
		case EBADF:
		case EFAULT:
		case EISCONN:
		case ENOTSOCK:
			throw std::system_error(errno, std::system_category(), "connect failed");
		default:
			LOG_WARN("creating stream buffer - failed to connect fd=" << fd << " ip_addr=" << ip_addr << " port=" << port << " errno=" << errno);
		}
	}

	return fd;
}

void Handle::Close() {
	close(fd_);
}

} /* namespace ael */
