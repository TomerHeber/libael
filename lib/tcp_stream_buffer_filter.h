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
	TCPStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, Handle handle, bool pending_connect);
	virtual ~TCPStreamBufferFilter();

	static std::shared_ptr<TCPStreamBufferFilter> Create(std::shared_ptr<StreamBuffer> stream_buffer, Handle handle, bool connected);

	friend std::ostream& operator<<(std::ostream &out, const TCPStreamBufferFilter *filter);

private:
	InResult In() override;
	OutResult Out(std::shared_ptr<const DataView> &data_view) override;
	ConnectResult Connect() override;
	ConnectResult Accept() override;
	ShutdownResult Shutdown() override;

	Handle handle_;
	bool pending_connect_;
};

} /* namespace ael */

#endif /* LIB_LINUX_TCP_STREAM_FILTER_H_ */
