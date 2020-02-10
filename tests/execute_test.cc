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

private:
	int count_;
	mutex mut_;
	condition_variable cond_;
};

TEST(Execute, Basic) {
	int count = 5;

	auto event_loop = EventLoop::Create();
	auto latch = make_shared<CountDownLatch>(count);

	for (auto i = 0; i < count; i++) {
		event_loop->Execute(&CountDownLatch::Dec, latch);
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
			event_loop->Execute(&CountDownLatch::Dec, latch);
		}
	}

	ASSERT_TRUE(latch->Wait(10000ms));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    log::Sink::sink_ = new CoutSink();

    return RUN_ALL_TESTS();
}
