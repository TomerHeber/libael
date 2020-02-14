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

class OutResult {
public:
	OutResult() : should_close_(false) {}

	bool ShouldCloseWrite() const { return should_close_; }

	static OutResult CreateShouldClose() { return OutResult(true); }

private:
	OutResult(bool should_close) : should_close_(should_close) {}

	bool should_close_;
};

class InResult {
public:
	InResult() : should_close_(true) {}

	bool ShouldCloseRead() const { return should_close_; }
	bool HasMore() const { return data_view.GetDataLength() > 0; }
	bool HasData() const { return data_view.GetDataLength() > 0; }
	DataView GetData() const { return data_view; }

	static InResult CreateWouldBlock() { return InResult(false); }
	static InResult CreateShouldClose() { return InResult(true); }
	static InResult CreateHasData(std::uint8_t *buf, std::uint32_t buf_size) { return InResult(buf, buf_size); }

private:
	InResult(std::uint8_t *buf, std::uint32_t buf_size) :
		should_close_(false), data_view(buf, buf_size) {}
	InResult(bool should_close) : should_close_(should_close) {}

	bool should_close_;
	DataView data_view;
};

class StreamBufferFilter {
public:
	StreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, bool connected_ = false);
	virtual ~StreamBufferFilter() {}

	friend std::ostream& operator<<(std::ostream &out, const StreamBufferFilter *filter);

	virtual void Connect() = 0;

	void Write(const std::list<std::shared_ptr<const DataView>> &write_list);
	void Read();
	void Close();
	int GetFlags() const;

	bool IsReadClosed() const { return read_closed_; }
	bool IsWriteClosed() const { return write_closed_; }
	bool IsConnected() const { return connected_ ; }

protected:
	bool HasPrev() const { return prev_ != nullptr; }
	bool HasNext() const { return next_ != nullptr; }

	StreamBufferFilter* Prev() const { return prev_; }
	StreamBufferFilter* Next() const { return next_; }

	void HandleData(const DataView &data_view);

	virtual InResult In(std::uint8_t *buf, std::uint32_t buf_size) = 0;
	virtual OutResult Out(std::list<std::shared_ptr<const DataView>> &out_list) = 0;

	bool connected_;
	bool read_closed_;
	bool write_closed_;

	std::list<std::shared_ptr<const DataView>> pending_out_;

private:
	StreamBufferFilter *prev_;
	StreamBufferFilter *next_;
	std::weak_ptr<StreamBuffer> stream_buffer_;
	const std::uint64_t id_;
	std::uint32_t order_;


	friend StreamBuffer;
};

class StreamBuffer: public EventHandler, public std::enable_shared_from_this<StreamBuffer> {
public:
	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd);
	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port);

	friend std::ostream& operator<<(std::ostream &out, const StreamBuffer *stream_buffer);

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
	bool IsConnected() const;
	bool IsReadClosed() const;
	bool IsWriteClosed() const;

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
