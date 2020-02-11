/*
 * stream_buffer.h
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#ifndef INCLUDE_STREAM_BUFFER_H_
#define INCLUDE_STREAM_BUFFER_H_

#include <memory>
#include <list>
#include <mutex>
#include <atomic>

#include <netinet/in.h>

#include "event.h"
#include "data_view.h"

namespace ael {

class StreamBuffer;

class StreamBufferHandler {
public:
	StreamBufferHandler() {}
	virtual ~StreamBufferHandler() {}

	virtual void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const DataView &data_view) = 0;
	virtual void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) = 0;
	virtual void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) = 0;
};

class StreamBuffer: public EventHandler, public std::enable_shared_from_this<StreamBuffer>  {
public:
	virtual ~StreamBuffer() {}

	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd);
	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port);

	void Write(const DataView &data_view);
	void Close();

private:
	StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd, bool connected);

	virtual void Handle(std::uint32_t events);
	virtual int GetFlags() const;

	void DoRead(std::uint32_t events, StreamBufferHandler *stream_buffer_handler);
	void DoWrite(std::uint32_t events, StreamBufferHandler *stream_buffer_handler);
	void DoFinalize(StreamBufferHandler *stream_buffer_handler);
	void CompleteConnect(std::uint32_t events, StreamBufferHandler *stream_buffer_handler);

	void DoReadHandleError(int error);
	void DoWriteHandleError(int error);

	std::weak_ptr<StreamBufferHandler> stream_buffer_handler_;
	std::list<std::shared_ptr<const DataView>> pending_writes_;
	std::mutex lock_;
	std::atomic_bool read_closed_; // should be atomic because it can be modified by multiple threads.
	bool write_closed_;
	bool eof_called_;
	bool connected_;
};

}

#endif /* INCLUDE_STREAM_BUFFER_H_ */
