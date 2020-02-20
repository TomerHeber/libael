/*
 * stream_buffer_linux.cc
 *
 *  Created on: Feb 20, 2020
 *      Author: tomer
 */

#include "stream_buffer.h"
#include "log.h"
#include "tcp_stream_buffer_filter.h"

#include <cstring>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

namespace ael {

std::shared_ptr<StreamBuffer> StreamBuffer::CreateForClient(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, std::uint16_t port) {
	sockaddr *addr = nullptr;
	socklen_t addr_len;
	sa_family_t domain;

	sockaddr_in in4 = {};
	sockaddr_in6 in6 = {};

	auto is_connected = false;

	if (inet_pton(AF_INET, ip_addr.c_str(), &in4.sin_addr) == 1) {
		in4.sin_family = AF_INET;
		in4.sin_port = htons(port);
		addr = reinterpret_cast<sockaddr*>(&in4);
		addr_len = sizeof(in4);
		domain = AF_INET;
	} else if (inet_pton(AF_INET6, ip_addr.c_str(), &in6.sin6_addr) == 1) {
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons(port);
		addr = reinterpret_cast<sockaddr*>(&in6);
		addr_len = sizeof(in6);
		domain = AF_INET6;
	} else {
		throw "invalid host - inet_pton failed for both IPv4 and IPv6";
	}

	auto fd = socket(domain, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		throw std::system_error(errno, std::system_category(), "socket failed");
	}

	LOG_TRACE("creating stream buffer - connecting fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);

	auto connect_ret = connect(fd, addr, addr_len);
	if (connect_ret == 0) {
		LOG_TRACE("creating stream buffer - connected fd=" << fd << " ip_addr=" << ip_addr << " port=" << port);
		is_connected = true;
	} else {
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
			LOG_WARN("creating stream buffer - failed to connect fd=" << fd << " ip_addr=" << ip_addr << " port=" << port << " error=" << std::strerror(errno));
		}
	}

	std::shared_ptr<StreamBuffer> stream_buffer(new StreamBuffer(stream_buffer_handler, fd, CLIENT_MODE));
	auto tcp_stream_buffer_filter = TCPStreamBufferFilter::Create(stream_buffer, fd, is_connected);
	stream_buffer->AddStreamBufferFilter(tcp_stream_buffer_filter);
	return stream_buffer;
}

}
