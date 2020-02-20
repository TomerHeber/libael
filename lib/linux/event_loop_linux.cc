/*
 * event_loop_linux.cc
 *
 *  Created on: Feb 19, 2020
 *      Author: tomer
 */

#include "event_loop.h"
#include "log.h"
#include "config.h"

#include <cstdint>

#include <unistd.h>

#include <sys/epoll.h>

namespace ael {

void EventLoop::TimerHandler::HandleEvents(Handle handle, std::uint32_t events) {
	if (canceled_) {
		LOG_TRACE("cannot handle timer canceled " << this);
		return;
	}

	if (!(events & EPOLLIN)) {
		LOG_WARN("received an unexpected events " << this << " events=" << events);
		return;
	}

	std::uint64_t occurrences;

	auto ret = read(handle, &occurrences, sizeof(occurrences));

	if (ret != sizeof(occurrences)) {
		switch (errno) {
		case EAGAIN:
			LOG_TRACE("timer has not expired " << this);
			return;
		default:
			throw std::system_error(errno, std::system_category(), "timerfd read - failed");
		}
	}

	auto instance = instance_.lock();
	if (!instance) {
		LOG_WARN("timer cannot be executed instance has been destroyed - stopping timer " << this)
		CloseEvent();
		return;
	}

	if (occurrences > GLOBAL_CONFIG.interval_occurrences_limit_) {
		LOG_WARN("too many stacked interval occurrences - reducing to " << GLOBAL_CONFIG.interval_occurrences_limit_ << " " << this)
		occurrences = GLOBAL_CONFIG.interval_occurrences_limit_;
	}

	for (std::uint32_t i = 0; i < occurrences; i++) {
		func_();
	}

	if (run_once_) {
		Cancel();
	}
}

}

