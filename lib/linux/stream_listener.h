/*
 * stream_listener.h
 *
 *  Created on: Feb 5, 2020
 *      Author: tomer
 */

#ifndef LIB_LINUX_STREAM_LISTENER_H_
#define LIB_LINUX_STREAM_LISTENER_H_

#include "event.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <memory>

namespace ael {

class StreamListener: public EventHandler {
public:
	StreamListener(int domain, const sockaddr *addr, socklen_t addr_size);
	virtual ~StreamListener() {}

private:
	virtual void Handle(std::shared_ptr<class Event> event, std::uint32_t events);
	virtual int GetFlags() const;
	virtual int GetFD() const;

	virtual void HandleNewConnection(int fd) = 0;

	int domain_;
	std::unique_ptr<std::uint8_t[]> addr_;
	socklen_t addr_size_;
};

}

#endif /* LIB_LINUX_STREAM_LISTENER_H_ */
