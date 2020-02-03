/*
 * async_io.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_ASYNC_IO_H_
#define LIB_ASYNC_IO_H_

#include <memory>

namespace ael {

class AsyncIO {
public:
	AsyncIO();
	virtual ~AsyncIO();

	static std::unique_ptr<AsyncIO> Create();

	virtual void Init() = 0;
};

}


#endif /* LIB_ASYNC_IO_H_ */
