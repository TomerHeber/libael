/*
 * tcp_test.cc
 *
 *  Created on: Feb 7, 2020
 *      Author: tomer
 */

#include "gtest/gtest.h"

#include "log.h"
#include "helpers.h"
#include "stream_listener.h"
#include "stream_buffer.h"
#include "event_loop.h"

#include <chrono>
#include <random>

using namespace ael;
using namespace std;

static random_device rd;
static mt19937 mt(rd());
static uniform_int_distribution<int> uniform_port_dist(10000, 60000);

class NewConnectionHandlerCount : public NewConnectionHandler, public WaitCount {
public:
	NewConnectionHandlerCount(int expected_connections_count, const chrono::milliseconds &wait_time) : WaitCount(expected_connections_count, wait_time) {}
	virtual ~NewConnectionHandlerCount() {}

	void HandleNewConnection(int fd) override {
		close(fd);
		Dec();
	}
};

class StreamBufferHandlerCount : public StreamBufferHandler, public WaitCount {
public:
	StreamBufferHandlerCount(int expected_count, const chrono::milliseconds &wait_time) : WaitCount(expected_count, wait_time) {}
	virtual ~StreamBufferHandlerCount() {}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const DataView &data_view) override {}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		Dec();
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		Dec();
	}
};

class StreamBufferHandlerPongCount : public StreamBufferHandler, public WaitCount, public std::enable_shared_from_this<StreamBufferHandlerPongCount> {
public:
	StreamBufferHandlerPongCount(int expected_count, const chrono::milliseconds &wait_time) : WaitCount(expected_count, wait_time) {
		event_loop_ = EventLoop::Create();
	}
	virtual ~StreamBufferHandlerPongCount() {}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		ASSERT_EQ(1, strings_.erase(stream_buffer));
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		auto ping_msg = string("ping");
		stream_buffer->Write(ping_msg.substr(0, 2));
		stream_buffer->Write(ping_msg.substr(2, 1));
		stream_buffer->Write(ping_msg.substr(3, 1));
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const DataView &data_view) override {
		string &str = strings_[stream_buffer];
		data_view.AppendToString(str);
		if (str == "pong") {
			Dec();
		} else if (str.length() > 4) {
			throw "string too long";
		}
	}

	void Connect(const string &host, in_port_t port) {
		auto stream_buffer = StreamBuffer::Create(shared_from_this(), host, port);
		strings_[stream_buffer] = "";
		event_loop_->Attach(stream_buffer);
	}

private:
	unordered_map<std::shared_ptr<StreamBuffer>,string> strings_;
	shared_ptr<EventLoop> event_loop_;
};

class PingServer : public NewConnectionHandler, public WaitCount, public StreamBufferHandler, public std::enable_shared_from_this<PingServer>  {
public:
	PingServer(int expected_connections_count, const chrono::milliseconds &wait_time) : WaitCount(expected_connections_count, wait_time) {
		event_loop_ = EventLoop::Create();
	}
	virtual ~PingServer() {}

	void HandleNewConnection(int fd) override {
		auto stream_buffer = StreamBuffer::Create(shared_from_this(), fd);
		strings_[stream_buffer] = "";
		event_loop_->Attach(stream_buffer);
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		ASSERT_EQ(1, strings_.erase(stream_buffer));
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		throw "should not occur - all connections should already be connected";
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, const DataView &data_view) override {
		string &str = strings_[stream_buffer];
		data_view.AppendToString(str);
		if (str == "ping") {
			auto pong_msg = string("pong");
			stream_buffer->Write(pong_msg);
			stream_buffer->Close();
		} else if (str.length() > 4) {
			throw "string too long";
		}
	}

private:
	unordered_map<std::shared_ptr<StreamBuffer>,string> strings_;
	shared_ptr<EventLoop> event_loop_;
};

TEST(Listener, Create) {
	in_port_t port = uniform_port_dist(mt);

	EXPECT_ANY_THROW(StreamListener::Create(nullptr, "434", 4));
	EXPECT_ANY_THROW(StreamListener::Create(nullptr, "fsdf", 4));
	EXPECT_ANY_THROW(StreamListener::Create(nullptr, "fsdfsd", 4));
	EXPECT_NO_THROW(StreamListener::Create(nullptr, "127.0.0.1", port));
	EXPECT_NO_THROW(StreamListener::Create(nullptr, "::1", port));
}

TEST(Listener, CreateAndAttach) {
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(-1, 1ms);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop = EventLoop::Create();
	event_loop->Attach(stream_listener);
	this_thread::sleep_for(5ms);
	stream_listener.reset();
	this_thread::sleep_for(5ms);
	event_loop.reset();
}

TEST(Listener, OneConnection) {
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(1, 1000ms);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop = EventLoop::Create();
	event_loop->Attach(stream_listener);

	auto connection1_fd = ConnectTo("127.0.0.1", port);
	ASSERT_GE(connection1_fd, 0);
	close(connection1_fd);

	ASSERT_TRUE(new_connection_handler->Wait());
}

TEST(Listener, ThreeConnections) {
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(3, 1000ms);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop = EventLoop::Create();
	event_loop->Attach(stream_listener);

	auto connection1_fd = ConnectTo("127.0.0.1", port);
	ASSERT_GE(connection1_fd, 0);
	close(connection1_fd);

	auto connection2_fd = ConnectTo("127.0.0.1", port);
	ASSERT_GE(connection2_fd, 0);
	close(connection2_fd);

	auto connection3_fd = ConnectTo("127.0.0.1", port);
	ASSERT_GE(connection3_fd, 0);
	close(connection3_fd);

	ASSERT_TRUE(new_connection_handler->Wait());
}

TEST(StreamBuffer, Basic) {
	auto count = 50;
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(count, 5000ms);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop1 = EventLoop::Create();
	event_loop1->Attach(stream_listener);

	auto event_loop2 = EventLoop::Create();
	auto stream_buffer_handler = make_shared<StreamBufferHandlerCount>(count * 2, 5000ms);

	vector<shared_ptr<StreamBuffer>> m_streams;

	for (auto i = 0; i < count; i++) {
		m_streams.push_back(StreamBuffer::Create(stream_buffer_handler, "127.0.0.1", port));
		event_loop2->Attach(m_streams.back());
	}

	ASSERT_TRUE(new_connection_handler->Wait());
	ASSERT_TRUE(stream_buffer_handler->Wait());
}

TEST(StreamBuffer, PingPong) {
	auto count = 30;
	in_port_t port = uniform_port_dist(mt);

	auto event_loop = EventLoop::Create();

	auto ping_server = make_shared<PingServer>(count, 10000ms);
	auto ping_server_listener = StreamListener::Create(ping_server, "127.0.0.1", port);
	event_loop->Attach(ping_server_listener);

	auto stream_buffer_handler = make_shared<StreamBufferHandlerPongCount>(count * 2, 10000ms);
	for (auto i = 0; i < count; i++) {
		stream_buffer_handler->Connect("127.0.0.1", port);
	}

	stream_buffer_handler->Wait();
	ping_server->Wait();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new Environment);

    return RUN_ALL_TESTS();
}
