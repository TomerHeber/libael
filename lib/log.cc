/*
 * log.cc
 *
 *  Created on: Feb 6, 2020
 *      Author: tomer
 */

#include "log.h"

namespace ael {

namespace log {

std::unique_ptr<Sink> Sink::sink_;
LogLevel Sink::log_level_ = LogLevel::None;
std::chrono::time_point<std::chrono::high_resolution_clock> Sink::start_time_ = std::chrono::high_resolution_clock::now();

}

}

