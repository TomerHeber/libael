/*
 * async_io.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_ASYNC_IO_H_
#define LIB_ASYNC_IO_H_

#include <memory>
#include <unordered_map>

#include "event.h"

namespace ael {

class AsyncIO {
public:
	AsyncIO() {}
	virtual ~AsyncIO() {}

	static std::unique_ptr<AsyncIO> Create();

	virtual void Add(std::shared_ptr<Event> event) = 0; // Add (register) an event.
	virtual void Modify(std::shared_ptr<Event> event) = 0; // Modify the "state" of the event.
	virtual void Ready(std::shared_ptr<Event> event, Events events) = 0; // Makes (an already registered) event ready (if no longer registers - should ignore).
	virtual void Remove(std::shared_ptr<Event> event) = 0; // Remove (unregister) an event.
	virtual void Wakeup() = 0; // Unblock "Process()".
	virtual void Process() = 0; // Process() "registered" events.
};

}


#endif /* LIB_ASYNC_IO_H_ */
