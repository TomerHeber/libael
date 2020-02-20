/*
 * stream_listener.cc
 *
 *  Created on: Feb 5, 2020
 *      Author: tomer
 */

#include "stream_listener.h"

#include "async_io.h"
#include "config.h"
#include "log.h"
/*
 *
 * #include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
 *
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/epoll.h>

#include <arpa/inet.h>
*/

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

}
