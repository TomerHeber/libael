/*
 * eventloop.h
 *
 *  Created on: Jan 31, 2020
 *      Author: tomer
 */

#ifndef LIB_EVENTLOOP_H_
#define LIB_EVENTLOOP_H_

#include <thread>
#include <memory>

#include "async_io.h"

namespace ael {

class EventLoop {
public:
	static std::shared_ptr<EventLoop> Create();

	virtual ~EventLoop();

private:
	EventLoop();

	void Run();

	std::unique_ptr<std::thread> thread_;
	std::unique_ptr<AsyncIO> async_io_;
	bool stop_;
};

}

#endif /* LIB_EVENTLOOP_H_ */
