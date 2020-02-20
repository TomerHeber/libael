/*
 * config.cc
 *
 *  Created on: Feb 6, 2020
 *      Author: tomer
 */

#include "config.h"

namespace ael {

Config Config::_config;

Config::Config() :
		listen_backlog_(128),
		listen_starvation_limit_(50),
		read_starvation_limit_(1048576),
		write_starvation_limit_(1048576),
		interval_occurrences_limit_(10)
		{}

Config::~Config() {}

} /* namespace ael */
