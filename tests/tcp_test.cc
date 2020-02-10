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
#include "event_loop.h"

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>

using namespace ael;
using namespace std;

static random_device rd;
static mt19937 mt(rd());
static uniform_int_distribution<int> uniform_port_dist(10000, 60000);

class NewConnectionHandlerCount : public NewConnectionHandler {
public:
	NewConnectionHandlerCount(int expectedConnections) : count_(expectedConnections) {}
	virtual ~NewConnectionHandlerCount() {}

	virtual void HandleNewConnection(int fd) {
		close(fd);
		mut_.lock();
		count_--;
		if (count_ == 0) {
			cond_.notify_one();
		}
		mut_.unlock();
	}

	bool Wait() {
		unique_lock<mutex> lock(mut_);
		return cond_.wait_for(lock, 200ms, [this]{return count_ == 0;});
	}

private:
	int count_;
	condition_variable cond_;
	mutex mut_;
};

TEST(Listener, Create)
{
	in_port_t port = uniform_port_dist(mt);

	EXPECT_ANY_THROW(StreamListener::Create(nullptr, "434", 4));
	EXPECT_ANY_THROW(StreamListener::Create(nullptr, "fsdf", 4));
	EXPECT_ANY_THROW(StreamListener::Create(nullptr, "fsdfsd", 4));
	EXPECT_NO_THROW(StreamListener::Create(nullptr, "127.0.0.1", port));
	EXPECT_NO_THROW(StreamListener::Create(nullptr, "::1", port));
}

TEST(Listener, CreateAndAttach)
{
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(-1);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop = EventLoop::Create();
	event_loop->Attach(stream_listener);
	this_thread::sleep_for(5ms);
	stream_listener.reset();
	this_thread::sleep_for(5ms);
	event_loop.reset();
}

TEST(Listener, OneConnection)
{
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(1);
	auto stream_listener = StreamListener::Create(new_connection_handler, "127.0.0.1", port);
	auto event_loop = EventLoop::Create();
	event_loop->Attach(stream_listener);

	ASSERT_FALSE(new_connection_handler->Wait());

	auto connection1_fd = ConnectTo("127.0.0.1", port);
	ASSERT_GE(connection1_fd, 0);
	close(connection1_fd);

	ASSERT_TRUE(new_connection_handler->Wait());
}

TEST(Listener, ThreeConnections)
{
	in_port_t port = uniform_port_dist(mt);

	auto new_connection_handler = make_shared<NewConnectionHandlerCount>(3);
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

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    log::Sink::sink_ = new CoutSink();

    return RUN_ALL_TESTS();
}
