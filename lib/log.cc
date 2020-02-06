/*
 * log.cc
 *
 *  Created on: Feb 6, 2020
 *      Author: tomer
 */

#include "log.h"

namespace ael {

namespace log {

Sink* Sink::sink_ = nullptr;
LogLevel Sink::log_level_ = LogLevel::None;

}

}

