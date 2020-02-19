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

	virtual void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const std::shared_ptr<const DataView> &data_view) = 0;
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
	InResult() : should_close_(false) {}
	InResult(const std::uint8_t *buf, std::uint32_t buf_size) : should_close_(false), data_view(DataView(buf, buf_size).Save()) {}

	bool ShouldCloseRead() const { return should_close_; }
	bool HasData() const { return data_view && data_view->GetDataLength() > 0; }
	std::shared_ptr<const DataView> GetData() const { return data_view; }

	static InResult CreateShouldClose() { return InResult(true); }

private:
	InResult(bool should_close) : should_close_(should_close) {}

	bool should_close_;
	std::shared_ptr<const DataView> data_view;
};

class ConnectResult {
public:
	ConnectResult() : status_(0) {}

	bool IsPending() const { return status_ == 0; }
	bool IsFailed() const { return status_ == 1; }
	bool IsSuccess() const { return status_ == 2; }

	static ConnectResult CreatePending() { return ConnectResult(0); }
	static ConnectResult CreateFailed() { return ConnectResult(1); }
	static ConnectResult CreateSuccess() { return ConnectResult(2); }

private:
	ConnectResult(std::uint8_t status) : status_(status) {}

	std::uint8_t status_;
};

class ShutdownResult {
public:
	ShutdownResult(bool complete) : complete_(complete) {}

	bool IsComplete() const { return complete_; }

private:
	bool complete_;
};

class StreamBufferFilter {
public:
	StreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer);
	virtual ~StreamBufferFilter() {}

	friend std::ostream& operator<<(std::ostream &out, const StreamBufferFilter *filter);

protected:
	bool HasPrev() const { return prev_ != nullptr; }
	bool HasNext() const { return next_ != nullptr; }

	InResult PrevIn() const { return prev_->In(); }
	OutResult PrevOut(std::shared_ptr<const DataView> &data_view) const { return prev_->Out(data_view); }

	void HandleData(const std::shared_ptr<const DataView> &data_view);

	virtual InResult In() = 0;
	virtual OutResult Out(std::shared_ptr<const DataView> &data_view) = 0;
	virtual ShutdownResult Shutdown() = 0;
	virtual ConnectResult Connect() = 0;
	virtual ConnectResult Accept() = 0;

private:
	bool connected_;
	bool read_closed_;
	bool write_closed_;
	StreamBufferFilter *prev_;
	StreamBufferFilter *next_;
	std::weak_ptr<StreamBuffer> stream_buffer_;
	const std::uint64_t id_;
	std::uint32_t order_;
	std::list<std::shared_ptr<const DataView>> pending_out_;

	void Write(const std::list<std::shared_ptr<const DataView>> &write_list);
	void Read();
	void Close();
	int GetFlags() const;

	friend StreamBuffer;
};

class StreamBuffer: public EventHandler, public std::enable_shared_from_this<StreamBuffer> {
public:
	static std::shared_ptr<StreamBuffer> CreateForClient(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd);
	static std::shared_ptr<StreamBuffer> CreateForClient(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, const std::string &ip_addr, in_port_t port);
	static std::shared_ptr<StreamBuffer> CreateForServer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd);

	friend std::ostream& operator<<(std::ostream &out, const StreamBuffer *stream_buffer);

	void Write(const DataView &data_view);
	void Close();
	void AddStreamBufferFilter(std::shared_ptr<StreamBufferFilter> stream_filter);

private:
	enum StreamBufferMode { SERVER_MODE, CLIENT_MODE };

	static std::shared_ptr<StreamBuffer> Create(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd, StreamBufferMode mode);

	StreamBuffer(std::shared_ptr<StreamBufferHandler> stream_buffer_handler, int fd, StreamBufferMode mode);

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
	StreamBufferMode mode_;

	friend StreamBufferFilter;
	//TODO IMPORTANT!!! handle starvation...
};

}

#endif /* INCLUDE_STREAM_BUFFER_H_ */
