/*
 * execute_test.cc
 *
 *  Created on: Feb 8, 2020
 *      Author: tomer
 */

#include <condition_variable>
#include <chrono>

#include "gtest/gtest.h"

#include "log.h"
#include "helpers.h"
#include "event_loop.h"

using namespace std;
using namespace ael;

class CountDownLatch {
public:
	CountDownLatch(int count) : count_(count) {}

	void Dec() {
		unique_lock<mutex> lock(mut_);

		count_--;

		if (count_== 0) {
			cond_.notify_one();
		}
	}

	template< class Rep, class Period>
	bool Wait(const chrono::duration<Rep, Period> &wait_time) {
		unique_lock<mutex> lock(mut_);

		if (count_ == 0) {
			return true;
		}

		return cond_.wait_for(lock, wait_time) == cv_status::no_timeout;
	}

	int GetCount() const {
		return count_;
	}

private:
	atomic_int count_;
	mutex mut_;
	condition_variable cond_;
};

TEST(Execute, Basic) {
	int count = 5;

	auto event_loop = EventLoop::Create();
	auto latch = make_shared<CountDownLatch>(count);

	for (auto i = 0; i < count; i++) {
		event_loop->ExecuteOnce(&CountDownLatch::Dec, latch);
	}

	ASSERT_TRUE(latch->Wait(5000ms));
}

TEST(Execute, Advanced) {
	int count = 250;
	int event_loop_count = 50;

	vector<shared_ptr<EventLoop>> event_loops;

	for (auto i = 0; i < event_loop_count; i++) {
		event_loops.push_back(EventLoop::Create());
	}

	auto latch = make_shared<CountDownLatch>(count * event_loop_count);

	for (auto i = 0; i < count; i++) {
		for (auto event_loop : event_loops) {
			event_loop->ExecuteOnce(&CountDownLatch::Dec, latch);
		}
	}

	ASSERT_TRUE(latch->Wait(10000ms));
}

TEST(ExecuteIn, Basic) {
	auto event_loop = EventLoop::Create();
	auto latch = make_shared<CountDownLatch>(2);

	event_loop->ExecuteOnceIn(500ms, &CountDownLatch::Dec, latch);
	event_loop->ExecuteOnceIn(1s, &CountDownLatch::Dec, latch);
	this_thread::sleep_for(250ms);
	ASSERT_EQ(2, latch->GetCount());
	this_thread::sleep_for(500ms);
	ASSERT_EQ(1, latch->GetCount());
	ASSERT_TRUE(latch->Wait(1250ms));
}

TEST(ExecuteInterval, Basic) {
	auto event_loop = EventLoop::Create();
	auto latch = make_shared<CountDownLatch>(5);
	auto timer = event_loop->ExecuteInterval(10ms, &CountDownLatch::Dec, latch);
	ASSERT_TRUE(latch->Wait(75ms));
	timer->Cancel();
}

TEST(ExecuteInterval, Cancel) {
	auto event_loop = EventLoop::Create();
	auto latch = make_shared<CountDownLatch>(10);
	auto timer = event_loop->ExecuteInterval(10ms, &CountDownLatch::Dec, latch);
	this_thread::sleep_for(40ms);
	timer->Cancel();
	this_thread::sleep_for(60ms);
	ASSERT_GE(latch->GetCount(), 4);
}

TEST(ExecuteIntervalIn, Basic) {
	auto event_loop = EventLoop::Create();
	auto latch = make_shared<CountDownLatch>(5);
	auto timer = event_loop->ExecuteIntervalIn(10ms, 50ms, &CountDownLatch::Dec, latch);
	this_thread::sleep_for(40ms);
	ASSERT_EQ(5, latch->GetCount());
	ASSERT_TRUE(latch->Wait(75ms));
	timer->Cancel();
}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new Environment);

    return RUN_ALL_TESTS();
}
