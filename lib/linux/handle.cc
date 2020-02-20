/*
 * Handle.cc
 *
 *  Created on: Feb 19, 2020
 *      Author: tomer
 */

#include "handle.h"
#include "log.h"
#include "config.h"

#include <unistd.h>

#include <sys/timerfd.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

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

Handle Handle::CreateStreamListenerHandle(const std::string &ip_addr, std::uint16_t port) {
	sockaddr *addr = nullptr;
	std::size_t addr_size = 0;
	int domain = 0;

	// Try IPv4.
	sockaddr_in in4 = {};
	if (inet_pton(AF_INET, ip_addr.c_str(), &in4.sin_addr) == 1) {
		in4.sin_family = AF_INET;
		in4.sin_port = htons(port);
		addr = reinterpret_cast<sockaddr*>(&in4);
		addr_size = sizeof(in4);
		domain = AF_INET;
	}

	// Try IPv6.
	sockaddr_in6 in6 = {};
	if (addr == nullptr && inet_pton(AF_INET6, ip_addr.c_str(), &in6.sin6_addr) == 1) {
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons(port);
		addr = reinterpret_cast<sockaddr*>(&in6);
		addr_size = sizeof(in6);
		domain = AF_INET6;
	}

	if (addr == nullptr) {
		throw "invalid host - inet_pton failed for both IPv4 and IPv6";
	}

	auto fd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "socket failed");
	}

	if (bind(fd, addr, addr_size) != 0) {
		throw std::system_error(errno, std::system_category(), "bind failed");
	}

	if (listen(fd, GLOBAL_CONFIG.listen_backlog_) != 0) {
		throw std::system_error(errno, std::system_category(), "listen failed");
	}

	LOG_TRACE("created descriptor for listener " << "fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);

	return fd;
}

void Handle::Close() {
	close(fd_);
}

} /* namespace ael */
