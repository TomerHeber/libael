/*
 * handle.h
 *
 *  Created on: Feb 19, 2020
 *      Author: tomer
 */

#ifndef INCLUDE_HANDLE_H_
#define INCLUDE_HANDLE_H_

#include <ostream>
#include <chrono>

namespace ael {

class Handle {
public:
	Handle() : fd_(-1) {}
	Handle(int fd) : fd_(fd) {}
	virtual ~Handle() {}

	operator int() const { return fd_; }
	explicit operator bool() const { return fd_ >= 0; }

	friend std::ostream& operator<<(std::ostream &out, const Handle handle);

	static Handle CreateTimerHandle(const std::chrono::nanoseconds &interval, const std::chrono::nanoseconds &value);
	static Handle CreateStreamListenerHandle(const std::string &ip_addr, std::uint16_t port);

	void Close();
private:
	int fd_;
};

} /* namespace ael */

#endif /* INCLUDE_HANDLE_H_ */
