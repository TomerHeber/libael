/*
 * async_io.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_ASYNC_IO_H_
#define LIB_ASYNC_IO_H_

#include <memory>

#include "event.h"

namespace ael {

#define READ_FLAG 		0x1
#define WRITE_FLAG 		0x2
#define STREAM_FLAG 	0x4

class AsyncIO {
public:
	AsyncIO() {}
	virtual ~AsyncIO() {}

	static std::unique_ptr<AsyncIO> Create();

	virtual void Add(std::shared_ptr<Event> event) = 0;
	virtual void Remove(std::shared_ptr<Event> event) = 0;

	virtual void Process() = 0;
};

}


#endif /* LIB_ASYNC_IO_H_ */
