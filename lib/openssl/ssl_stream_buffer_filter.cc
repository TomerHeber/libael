/*
 * ssl_stream_buffer_filter.cc
 *
 *  Created on: Feb 13, 2020
 *      Author: tomer
 */

#include "ssl_stream_buffer_filter.h"

namespace ael {

SSLStreamBufferFilter::SSLStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer, SSL *ssl) :
		StreamBufferFilter(stream_buffer), ssl_(ssl), rbio_(BIO_new(BIO_s_mem())), wbio_(BIO_new(BIO_s_mem())) {
	SSL_set0_rbio(ssl_, rbio_);
	SSL_set0_wbio(ssl_, wbio_);
}

SSLStreamBufferFilter::~SSLStreamBufferFilter() {
	SSL_free(ssl_);
}

} /* namespace ael */
