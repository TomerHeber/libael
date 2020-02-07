/*
 * stream_listener.h
 *
 *  Created on: Feb 5, 2020
 *      Author: tomer
 */

#ifndef LIB_LINUX_STREAM_LISTENER_H_
#define LIB_LINUX_STREAM_LISTENER_H_

#include "event.h"

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <memory>

namespace ael {

class StreamListener: public EventHandler {
public:

	virtual ~StreamListener() {}

	static std::shared_ptr<StreamListener> Create(std::shared_ptr<NewConnectionHandler> new_connection_handler, const std::string &ip_addr, in_port_t port);

private:
	StreamListener(std::shared_ptr<NewConnectionHandler> new_connection_handler, int domain, const sockaddr *addr, socklen_t addr_size);

	virtual void Handle(std::shared_ptr<class Event> event, std::uint32_t events);
	virtual int GetFlags() const;
	virtual int GetFD() const;

	std::weak_ptr<NewConnectionHandler> new_connection_handler_;
	int domain_;
	std::unique_ptr<std::uint8_t[]> addr_;
	socklen_t addr_size_;
};

}

#endif /* LIB_LINUX_STREAM_LISTENER_H_ */
