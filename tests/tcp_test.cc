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
#include <algorithm>

using namespace ael;
using namespace std;

static thread_local random_device rd;
static thread_local mt19937_64 mt(rd());
static thread_local uniform_int_distribution<int> uniform_port_dist(10000, 60000);

class DummyStreamBufferFilter: public StreamBufferFilter {
public:
	DummyStreamBufferFilter(std::shared_ptr<StreamBuffer> stream_buffer) : StreamBufferFilter(stream_buffer), close_next_in_(false) {}
	virtual ~DummyStreamBufferFilter() {}

	friend std::ostream& operator<<(std::ostream &out, const DummyStreamBufferFilter *filter) {
		const StreamBufferFilter *stream_buffer_filter = filter;
		out << stream_buffer_filter;
		return out;
	}

private:
	InResult In() override {
		if (close_next_in_) {
			LOG_TRACE("close_next_in proceeding to shutdown " << this)
			return InResult::CreateShouldClose();
		}

		auto in_result = PrevIn();

		if (in_result.ShouldCloseRead()) {
			LOG_TRACE("in result should close read " << this)
			return in_result;
		}

		if (!in_result.HasData()) {
			LOG_TRACE("in result no data " << this)
			return in_result;
		}

		string in_str;
		in_result.GetData()->AppendToString(in_str);
		in_str.erase(std::remove(in_str.begin(), in_str.end(), '*'), in_str.end());

		LOG_TRACE("in result data: " << in_str << " removing all * " << this);

		if (in_str.find('#') != string::npos) {
			LOG_TRACE("other side is closing (found #)" << this);
			in_str.erase(std::remove(in_str.begin(), in_str.end(), '#'), in_str.end());
			if (in_str.empty()) {
				LOG_TRACE("shut down now string empty " << this)
				return InResult::CreateShouldClose();
			} else {
				LOG_TRACE("shut down later string not empty " << this)
				close_next_in_ = true;
			}
		}

		LOG_TRACE("return in result data: " << in_str << " " << this);

		return InResult(reinterpret_cast<const std::uint8_t*>(in_str.c_str()), in_str.length());
	}

	OutResult Out(std::list<std::shared_ptr<const DataView>> &out_list) override {
		string out;

		for (auto data_view : out_list) {
			data_view->AppendToString(out);
		}

		out_list.clear();

		LOG_TRACE("received from next: " << out << " " << this)

		out_list.push_back(DataView("*").Save());
		out_list.push_back(DataView(out).Save());
		out_list.push_back(DataView("*").Save());

		LOG_TRACE("out forwarding to prev: * " << out << " * " << this)

		auto out_result = PrevOut(out_list);

		if (out_result.ShouldCloseWrite()) {
			LOG_TRACE("out result should close write " << this);
			return out_result;
		}

		if (IsReadClosed() && out_list.empty()) {
			LOG_TRACE("read is closed and everything has been flushed send close signal #" << this)
			out_list.push_back(DataView("#").Save());
			auto out_result_inner = PrevOut(out_list);
			if (out_list.empty() || out_result_inner.ShouldCloseWrite()) {
				return OutResult::CreateShouldClose();
			}
		}

		return out_result;
	}

	ConnectResult Connect() override {
		return ConnectResult::CreateSuccess();
	}

	ConnectResult Accept() override {
		return ConnectResult::CreateSuccess();
	}

	ShutdownResult Shutdown() override {
		if (shut_down_received_) {
			std::list<std::shared_ptr<const DataView>> out_list;
			out_list.push_back(DataView("#").Save());
			auto out_result = PrevOut(out_list);
		}
		return ShutdownResult(true);
	}

	bool close_next_in_;
	bool shut_down_received_;

};

class DummyFilterServer : public NewConnectionHandler, public WaitCount, public StreamBufferHandler, public std::enable_shared_from_this<DummyFilterServer>  {
public:
	DummyFilterServer(int expected_connections_count, const chrono::milliseconds &wait_time) : WaitCount(expected_connections_count, wait_time) {
		event_loop_ = EventLoop::Create();
	}
	virtual ~DummyFilterServer() {}

	void HandleNewConnection(int fd) override {
		auto stream_buffer = StreamBuffer::CreateForServer(shared_from_this(), fd);
		BufferState buffer_state;
		buffer_state.upgraded = false;
		buffers_[stream_buffer] = buffer_state;
		event_loop_->Attach(stream_buffer);
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		ASSERT_EQ(1, buffers_.erase(stream_buffer));
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		BufferState &buffer_state = buffers_[stream_buffer];

		if (!buffer_state.upgraded) {
			LOG_TRACE("adding filter to stream_buffer " << stream_buffer);
			buffer_state.upgraded = true;
			auto dummy_filter = std::make_shared<DummyStreamBufferFilter>(stream_buffer);
			stream_buffer->AddStreamBufferFilter(dummy_filter);
		}
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, std::shared_ptr<const DataView> &data_view) override {
		BufferState &buffer_state = buffers_[stream_buffer];
		string &str = buffer_state.buf;

		data_view->AppendToString(str);

		LOG_TRACE("received " << str << " " << stream_buffer);

		if (str == "hello john") {
			str.clear();
			auto msg = string("hello jane");
			stream_buffer->Write(msg);
		} else if (str == "goodbye john") {
			auto msg = string("goodbye jane");
			stream_buffer->Write(msg);
			Dec();
		}
	}

private:
	struct BufferState {
		bool upgraded;
		string buf;
	};

	unordered_map<std::shared_ptr<StreamBuffer>,BufferState> buffers_;
	shared_ptr<EventLoop> event_loop_;
};

fsdfsdf //TODO --- fix thread safety in tests and run helgrind!
fdsfdf // TODO --- fix shutdown in filter.
fdsfsdf // TODO --- remove is ReadClosed... is WriteClosed getters

class DummyFilterClient : public StreamBufferHandler, public WaitCount, public std::enable_shared_from_this<DummyFilterClient> {
public:
	DummyFilterClient(int expected_count, const chrono::milliseconds &wait_time) : WaitCount(expected_count, wait_time) {
		event_loop_ = EventLoop::Create();
	}
	virtual ~DummyFilterClient() {}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		ASSERT_EQ(1, buffers_.erase(stream_buffer));
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		BufferState &buffer_state = buffers_[stream_buffer];

		if (!buffer_state.upgraded) {
			LOG_TRACE("adding filter to stream_buffer " << stream_buffer);
			buffer_state.upgraded = true;
			auto dummy_filter = std::make_shared<DummyStreamBufferFilter>(stream_buffer);
			stream_buffer->AddStreamBufferFilter(dummy_filter);
		} else {
			auto hello_john_msg = string("hello john");
			LOG_TRACE("writing hello john " << stream_buffer);
			stream_buffer->Write(hello_john_msg.substr(0, 2));
			stream_buffer->Write(hello_john_msg.substr(2, 1));
			stream_buffer->Write(hello_john_msg.substr(3));
		}
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, std::shared_ptr<const DataView> &data_view) override {
		BufferState &buffer_state = buffers_[stream_buffer];
		string &str = buffer_state.buf;

		data_view->AppendToString(str);

		LOG_TRACE("received " << str << " " << stream_buffer);

		if (str == "hello jane") {
			str.clear();
			auto msg = string("goodbye john");
			stream_buffer->Write(msg);
		} else if (str == "goodbye jane") {
			stream_buffer->Close();
			Dec();
		}
	}

	void Connect(const string &host, in_port_t port) {
		auto stream_buffer = StreamBuffer::CreateForClient(shared_from_this(), host, port);
		BufferState buffer_state;
		buffer_state.upgraded = false;
		buffers_[stream_buffer] = buffer_state;
		event_loop_->Attach(stream_buffer);
	}

private:
	struct BufferState {
		bool upgraded;
		string buf;
	};

	unordered_map<std::shared_ptr<StreamBuffer>,BufferState> buffers_;
	shared_ptr<EventLoop> event_loop_;
};

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

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, std::shared_ptr<const DataView> &data_view) override {}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		Dec();
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		Dec();
	}
};

class StreamBufferHandlerEOFCount : public StreamBufferHandler, public WaitCount {
public:
	StreamBufferHandlerEOFCount(int expected_count, const chrono::milliseconds &wait_time) : WaitCount(expected_count, wait_time) {}
	virtual ~StreamBufferHandlerEOFCount() {}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, std::shared_ptr<const DataView> &data_view) override {}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		throw "should not be able to successfully connect";
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
		LOG_TRACE("writing ping");
		stream_buffer->Write(ping_msg.substr(0, 2));
		stream_buffer->Write(ping_msg.substr(2, 1));
		stream_buffer->Write(ping_msg.substr(3, 1));
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, std::shared_ptr<const DataView> &data_view) override {
		string &str = strings_[stream_buffer];
		data_view->AppendToString(str);
		if (str == "pong") {
			LOG_TRACE("received pong");
			Dec();
		} else if (str.length() > 4) {
			throw "string too long";
		}
	}

	void Connect(const string &host, in_port_t port) {
		auto stream_buffer = StreamBuffer::CreateForClient(shared_from_this(), host, port);
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
		auto stream_buffer = StreamBuffer::CreateForServer(shared_from_this(), fd);
		strings_[stream_buffer] = "";
		event_loop_->Attach(stream_buffer);
	}

	void HandleEOF(std::shared_ptr<StreamBuffer> stream_buffer) override {
		ASSERT_EQ(1, strings_.erase(stream_buffer));
		Dec();
	}

	void HandleConnected(std::shared_ptr<StreamBuffer> stream_buffer) override {
		Dec();
	}

	void HandleData(std::shared_ptr<StreamBuffer> stream_buffer, std::shared_ptr<const DataView> &data_view) override {
		string &str = strings_[stream_buffer];
		data_view->AppendToString(str);
		if (str == "ping") {
			LOG_TRACE("received ping writing pong");
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

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(count, 2000ms);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop1 = EventLoop::Create();
	event_loop1->Attach(stream_listener);

	auto event_loop2 = EventLoop::Create();
	auto stream_buffer_handler = make_shared<StreamBufferHandlerCount>(count * 2, 2000ms);

	vector<shared_ptr<StreamBuffer>> m_streams;

	for (auto i = 0; i < count; i++) {
		m_streams.push_back(StreamBuffer::CreateForClient(stream_buffer_handler, "127.0.0.1", port));
		event_loop2->Attach(m_streams.back());
	}

	ASSERT_TRUE(new_connection_handler->Wait());
	ASSERT_TRUE(stream_buffer_handler->Wait());
}

TEST(StreamBuffer, PingPong) {
	auto count = 50;
	in_port_t port = uniform_port_dist(mt);

	auto event_loop = EventLoop::Create();

	auto ping_server = make_shared<PingServer>(count * 2, 2000ms);
	auto ping_server_listener = StreamListener::Create(ping_server, "127.0.0.1", port);
	event_loop->Attach(ping_server_listener);

	auto stream_buffer_handler = make_shared<StreamBufferHandlerPongCount>(count * 2, 2000ms);
	for (auto i = 0; i < count; i++) {
		stream_buffer_handler->Connect("127.0.0.1", port);
	}

	stream_buffer_handler->Wait();
	ping_server->Wait();
}

TEST(StreamBuffer, ConnectFailure) {
	auto event_loop = EventLoop::Create();
	auto stream_buffer_handler = make_shared<StreamBufferHandlerEOFCount>(1, 1000ms);
	auto stream_buffer = StreamBuffer::CreateForClient(stream_buffer_handler, "127.0.0.1", 999);
	event_loop->Attach(stream_buffer);
	stream_buffer_handler->Wait();
}

TEST(StreamBuffer, DummyFilter) {
	auto count = 50;
	in_port_t port = uniform_port_dist(mt);

	auto event_loop = EventLoop::Create();

	auto server = make_shared<DummyFilterServer>(count * 2, 2000ms);
	auto server_listener = StreamListener::Create(server, "127.0.0.1", port);
	event_loop->Attach(server_listener);

	auto client_handler = make_shared<DummyFilterClient>(count * 2, 2000ms);
	for (auto i = 0; i < count; i++) {
		client_handler->Connect("127.0.0.1", port);
	}

	client_handler->Wait();
	server->Wait();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new Environment);

    return RUN_ALL_TESTS();
}
