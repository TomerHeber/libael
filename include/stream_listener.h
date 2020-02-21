/*
 * stream_listener.h
 *
 *  Created on: Feb 5, 2020
 *      Author: tomer
 */

#ifndef LIB_LINUX_STREAM_LISTENER_H_
#define LIB_LINUX_STREAM_LISTENER_H_

#include "event.h"

#include <memory>

namespace ael {

class NewConnectionHandler {
public:
	NewConnectionHandler() {}
	virtual ~NewConnectionHandler() {}

	virtual void HandleNewConnection(Handle handle) = 0;
};

class StreamListener: public EventHandler {
public:
	virtual ~StreamListener() {}

	friend std::ostream& operator<<(std::ostream &out, const StreamListener *stream_listener);

	static std::shared_ptr<StreamListener> Create(std::shared_ptr<NewConnectionHandler> new_connection_handler, const std::string &ip_addr, std::uint16_t port);

private:
	StreamListener(std::shared_ptr<NewConnectionHandler> new_connection_handler, Handle handle);

	void HandleEvents(Handle handle, Events events) override;
	Events GetEvents() const override;

	std::weak_ptr<NewConnectionHandler> new_connection_handler_;
};

}

#endif /* LIB_LINUX_STREAM_LISTENER_H_ */
