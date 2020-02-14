/*
 * stream_buffer.h
 *
 *  Created on: Feb 9, 2020
 *      Author: tomer
 */

#ifndef INCLUDE_STREAM_BUFFER_H_
#define INCLUDE_STREAM_BUFFER_H_

#include <list>
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

class StreamBufferFilter {
public:
	StreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer) : prev_(nullptr), next_(nullptr), stream_buffer_(stream_buffer) {}
	virtual ~StreamBufferFilter() {}

	virtual void Write(const std::list<std::shared_ptr<const DataView>> &write_list) = 0;
	virtual void Read() = 0;
	virtual void Close() = 0;
	virtual void Connect() = 0;
	virtual int GetFlags() const = 0;
	virtual bool IsReadClosed() const = 0;
	virtual bool IsWriteClosed() const = 0;
	virtual bool IsConnected() const = 0;

protected:
	bool HasPrev() const { return prev_ != nullptr; }
	bool HasNext() const { return next_ != nullptr; }

	StreamBufferFilter* Prev() const { return prev_; }
	StreamBufferFilter* Next() const { return next_; }

	void HandleData(const DataView &data_view);

private:
	StreamBufferFilter *prev_;
	StreamBufferFilter *next_;
	std::weak_ptr<StreamBuffer> stream_buffer_;

	friend StreamBuffer;
};

class StreamBuffer: public EventHandler, public std::enable_shared_from_this<StreamBuffer> {
public:
	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd);
	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port);

	void Write(const DataView &data_view);
	void Close();
	void AddStreamBufferFilter(std::shared_ptr<StreamBufferFilter> stream_filter);

private:
	StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd);

	void Handle(std::uint32_t events) override;
	int GetFlags() const override;

	void DoRead();
	void DoWrite();
	void DoClose();
	void DoConnect(std::shared_ptr<StreamBufferHandler> stream_buffer_handler);
	void DoFinalize(std::shared_ptr<StreamBufferHandler> stream_buffer_handler);
	bool IsConnected() const { return stream_filters_.back()->IsConnected(); }

	std::weak_ptr<StreamBufferHandler> stream_buffer_handler_;
	std::list<std::shared_ptr<StreamBufferFilter>> stream_filters_;
	std::list<std::shared_ptr<const DataView>> pending_writes_;
	std::mutex pending_writes_lock_;
	bool add_filter_allowed_;
	bool eof_called_;
	std::atomic_bool should_close_;

	friend StreamBufferFilter;
	//TODO IMPORTANT!!! handle starvation...
};

}

#endif /* INCLUDE_STREAM_BUFFER_H_ */
