/*
 * ssl_stream_buffer_filter.h
 *
 *  Created on: Feb 13, 2020
 *      Author: tomer
 */

#ifndef LIB_OPENSSL_SSL_STREAM_BUFFER_FILTER_H_
#define LIB_OPENSSL_SSL_STREAM_BUFFER_FILTER_H_

#include "stream_buffer.h"

#include <openssl/ssl.h>

namespace ael {

class SSLStreamBufferFilter: public StreamBufferFilter {
public:
	SSLStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, SSL *ssl);
	virtual ~SSLStreamBufferFilter();

	void Connect() override;

private:
	InResult In(std::uint8_t *buf, std::uint32_t buf_size) override;
	OutResult Out(std::list<std::shared_ptr<const DataView>> &out_list) override;

	SSL *ssl_;
	BIO *rbio_;
	BIO *wbio_;
};

} /* namespace ael */

#endif /* LIB_OPENSSL_SSL_STREAM_BUFFER_FILTER_H_ */
