/*
 * ssl_test.cc
 *
 *  Created on: Feb 18, 2020
 *      Author: tomer
 */

#include "gtest/gtest.h"

#include "log.h"
#include "helpers.h"
#include "stream_listener.h"
#include "stream_buffer.h"
#include "ssl_stream_buffer_filter.h"
#include "event_loop.h"

#include <chrono>
#include <random>
#include <algorithm>

using namespace ael;
using namespace std;

static thread_local random_device rd;
static thread_local mt19937_64 mt(rd());
static thread_local uniform_int_distribution<int> uniform_port_dist(10000, 60000);

class BasicSSLServer : public NewConnectionHandler, public WaitCount, public StreamBufferHandler, public std::enable_shared_from_this<BasicSSLServer>  {
public:
	BasicSSLServer(int expected_connections_count, const chrono::milliseconds &wait_time, int order) : WaitCount(expected_connections_count, wait_time), order_(order) {
		event_loop_ = EventLoop::Create();
		ssl_ctx_ = SSL_CTX_new(SSLv23_server_method());

	    if (SSL_CTX_use_certificate_file(ssl_ctx_, "fake1.crt", SSL_FILETYPE_PEM) <= 0) {
	    	throw "failed to load certificate";
	    }

	    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, "fake1.key", SSL_FILETYPE_PEM) <= 0 ) {
	    	throw "failed to load key";
	    }
	}
	virtual ~BasicSSLServer() {
		SSL_CTX_free(ssl_ctx_);
	}

	void HandleNewConnection(int fd) override {
		auto stream_buffer = StreamBuffer::CreateForServer(shared_from_this(), fd);
		BufferState buffer_state;
		buffer_state.order = order_;
		lock_.lock();
		buffers_[stream_buffer] = buffer_state;
		lock_.unlock();
		event_loop_->Attach(stream_buffer);
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		lock_.lock();
		ASSERT_EQ(1, buffers_.erase(stream_buffer));
		lock_.unlock();
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		lock_.lock();
		BufferState &buffer_state = buffers_[stream_buffer];
		lock_.unlock();

		if (buffer_state.order > 0) {
			LOG_TRACE("adding ssl filter to stream_buffer " << stream_buffer);
			buffer_state.order--;
			auto ssl = SSL_new(ssl_ctx_);
			auto ssl_filter = std::make_shared<SSLStreamBufferFilter>(stream_buffer, ssl);
			stream_buffer->AddStreamBufferFilter(ssl_filter);
		}
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const std::shared_ptr<const DataView> &data_view) override {
		lock_.lock();
		BufferState &buffer_state = buffers_[stream_buffer];
		lock_.unlock();
		string &str = buffer_state.buf;

		data_view->AppendToString(str);

		LOG_TRACE("received " << str << " " << stream_buffer);

		if (str == "ping") {
			Dec();
			str.clear();
			LOG_TRACE("writing pong " << stream_buffer);
			auto msg = string("pong");
			stream_buffer->Write(msg);
			stream_buffer->Close();
		}
	}

private:
	struct BufferState {
		int order;
		string buf;
	};

	const int order_;
	mutex lock_;
	unordered_map<std::shared_ptr<StreamBuffer>,BufferState> buffers_;
	shared_ptr<EventLoop> event_loop_;
	SSL_CTX *ssl_ctx_;
};

class BasicSSLClient : public StreamBufferHandler, public WaitCount, public std::enable_shared_from_this<BasicSSLClient> {
public:
	BasicSSLClient(int expected_count, const chrono::milliseconds &wait_time, int order) : WaitCount(expected_count, wait_time), order_(order) {
		event_loop_ = EventLoop::Create();
		ssl_ctx_ = SSL_CTX_new(SSLv23_client_method());
	}
	virtual ~BasicSSLClient() {
		SSL_CTX_free(ssl_ctx_);
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		lock_.lock();
		ASSERT_EQ(1, buffers_.erase(stream_buffer));
		lock_.unlock();
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		lock_.lock();
		BufferState &buffer_state = buffers_[stream_buffer];
		lock_.unlock();

		if (buffer_state.order > 0) {
			LOG_TRACE("adding ssl filter to stream_buffer " << stream_buffer);
			buffer_state.order--;
			auto ssl = SSL_new(ssl_ctx_);
			auto ssl_filter = std::make_shared<SSLStreamBufferFilter>(stream_buffer, ssl);
			stream_buffer->AddStreamBufferFilter(ssl_filter);
		} else {
			auto msg = string("ping");
			LOG_TRACE("writing ping " << stream_buffer);
			stream_buffer->Write(msg);
		}
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const std::shared_ptr<const DataView> &data_view) override {
		lock_.lock();
		BufferState &buffer_state = buffers_[stream_buffer];
		lock_.unlock();
		string &str = buffer_state.buf;

		data_view->AppendToString(str);

		LOG_TRACE("received " << str << " " << stream_buffer);

		if (str == "pong") {
			str.clear();
			stream_buffer->Close();
			Dec();
		}
	}

	void Connect(const string &host, in_port_t port) {
		auto stream_buffer = StreamBuffer::CreateForClient(shared_from_this(), host, port);
		BufferState buffer_state;
		buffer_state.order = order_;
		lock_.lock();
		buffers_[stream_buffer] = buffer_state;
		lock_.unlock();
		event_loop_->Attach(stream_buffer);
	}

private:
	struct BufferState {
		int order;
		string buf;
	};

	const int order_;
	mutex lock_;
	unordered_map<std::shared_ptr<StreamBuffer>,BufferState> buffers_;
	shared_ptr<EventLoop> event_loop_;
	SSL_CTX *ssl_ctx_;
};

TEST(OpenSSLFilter, Basic) {
	auto count = 50;
	in_port_t port = uniform_port_dist(mt);

	auto event_loop = EventLoop::Create();

	auto server = make_shared<BasicSSLServer>(count * 2, 2000ms, 1);
	auto server_listener = StreamListener::Create(server, "127.0.0.1", port);
	event_loop->Attach(server_listener);

	auto client_handler = make_shared<BasicSSLClient>(count * 2, 2000ms, 1);
	for (auto i = 0; i < count; i++) {
		client_handler->Connect("127.0.0.1", port);
	}

	ASSERT_TRUE(client_handler->Wait());
	ASSERT_TRUE(server->Wait());
}

TEST(OpenSSLFilter, BasicLudicrous) {
	auto count = 5;
	in_port_t port = uniform_port_dist(mt);

	auto event_loop = EventLoop::Create();

	auto server = make_shared<BasicSSLServer>(count * 2, 5000ms, 25);
	auto server_listener = StreamListener::Create(server, "127.0.0.1", port);
	event_loop->Attach(server_listener);

	auto client_handler = make_shared<BasicSSLClient>(count * 2, 5000ms, 25);
	for (auto i = 0; i < count; i++) {
		client_handler->Connect("127.0.0.1", port);
	}

	ASSERT_TRUE(client_handler->Wait());
	ASSERT_TRUE(server->Wait());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new Environment);

    return RUN_ALL_TESTS();
}
