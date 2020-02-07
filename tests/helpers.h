/*
 * helpers.h
 *
 *  Created on: Feb 7, 2020
 *      Author: tomer
 */

#ifndef TESTS_HELPERS_H_
#define TESTS_HELPERS_H_

#include "log.h"

#include <netinet/in.h>

#include <string>

class CoutSink : public ael::log::Sink {
public:
	CoutSink() {}
	virtual ~CoutSink() {}

private:
	virtual void Log(ael::log::LogLevel log_level, const std::string &msg);
};

int ConnectTo(const std::string &ip_addr, in_port_t port);

#endif /* TESTS_HELPERS_H_ */
