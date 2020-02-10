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

#include "event.h"
#include "data_view.h"

namespace ael {

class StreamBuffer;

class StreamBufferHandler {
public:
	StreamBufferHandler() {}
	virtual ~StreamBufferHandler() {}

	virtual void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) = 0;
	virtual void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const DataView &data_view) = 0;
};

class StreamBuffer: public EventHandler, public std::enable_shared_from_this<StreamBuffer>  {
public:
	virtual ~StreamBuffer() {}

	static std::shared_ptr<StreamBuffer> Create(int fd, std::shared_ptr<StreamBufferHandler> stream_buffer_handler);

	void Write(const DataView &data_view);
	void Close();

private:
	StreamBuffer(int fd, std::shared_ptr<StreamBufferHandler> stream_buffer_handler);

	virtual void Handle(std::uint32_t events);
	virtual int GetFlags() const;

	void DoRead(std::uint32_t events, StreamBufferHandler *stream_buffer_handler);
	void DoWrite(std::uint32_t events);
	void DoFinalize(StreamBufferHandler *stream_buffer_handler);

	void DoReadHandleError();
	void DoWriteHandleError();

	std::weak_ptr<StreamBufferHandler> stream_buffer_handler_;
	std::list<std::shared_ptr<const DataView>> pending_writes_;
	std::mutex lock_;
	std::atomic_bool read_closed_; // should be atomic because it can be modified by multiple threads.
	bool write_closed_;
	bool eof_called_;
};

}

#endif /* INCLUDE_STREAM_BUFFER_H_ */
