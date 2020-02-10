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

class NewConnectionHandler {
public:
	NewConnectionHandler() {}
	virtual ~NewConnectionHandler() {}

	virtual void HandleNewConnection(int fd) = 0;
};

class StreamListener: public EventHandler {
public:
	virtual ~StreamListener() {}

	static std::shared_ptr<StreamListener> Create(std::shared_ptr<NewConnectionHandler> new_connection_handler, const std::string &ip_addr, in_port_t port);

private:
	StreamListener(std::shared_ptr<NewConnectionHandler> new_connection_handler, int fd);

	virtual void Handle(std::uint32_t events);
	virtual int GetFlags() const;

	std::weak_ptr<NewConnectionHandler> new_connection_handler_;
};

}

#endif /* LIB_LINUX_STREAM_LISTENER_H_ */