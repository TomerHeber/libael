/*
 * async_io_epoll.h
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */

#ifndef LIB_ASYNC_IO_EPOLL_H_
#define LIB_ASYNC_IO_EPOLL_H_

#include "async_io.h"

namespace ael {

class EPoll : public AsyncIO {
public:
	EPoll();
	virtual ~EPoll();

private:
	virtual void Init();

};

}

#endif /* LIB_ASYNC_IO_EPOLL_H_ */
