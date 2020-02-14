/*
 * tcp_stream_filter.h
 *
 *  Created on: Feb 12, 2020
 *      Author: tomer
 */

#ifndef LIB_LINUX_TCP_STREAM_FILTER_H_
#define LIB_LINUX_TCP_STREAM_FILTER_H_

#include "stream_buffer.h"

namespace ael {

class TCPStreamBufferFilter: public StreamBufferFilter {
public:
	TCPStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, int fd, int connected);
	virtual ~TCPStreamBufferFilter();

	static std::shared_ptr<TCPStreamBufferFilter> Create(std::shared_ptr<StreamBuffer> stream_buffer, int fd, bool is_connected);

	int GetHandler() const { return fd_; }

private:
	void Write(const std::list<std::shared_ptr<const DataView>> &write_list) override;
	void Read() override;
	void Close() override;
	void Connect() override;
	int GetFlags() const override;
	bool IsReadClosed() const override;
	bool IsWriteClosed() const override;
	bool IsConnected() const override;

	int fd_;
	int connected_;
	bool write_closed_;
	bool read_closed_;
	std::list<std::shared_ptr<const DataView>> pending_writes_;
};

} /* namespace ael */

#endif /* LIB_LINUX_TCP_STREAM_FILTER_H_ */
