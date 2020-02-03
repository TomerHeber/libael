/*
 * async_io.cpp
 *
 *  Created on: Feb 3, 2020
 *      Author: tomer
 */


#include "epoll.h"

namespace ael {

std::unique_ptr<AsyncIO> AsyncIO::Create() {
	return std::unique_ptr<AsyncIO>(new EPoll());
}

}
