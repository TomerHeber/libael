/*
 * helpers.cc
 *
 *  Created on: Feb 7, 2020
 *      Author: tomer
 */

#include "helpers.h"
#include "event_loop.h"

#include <iostream>
#include <cstring>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

using namespace ael;
using namespace std;

static string log_level_str[] {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"CRITICAL"
};

void CoutSink::Log(log::LogLevel log_level, const string &msg) {
	cout << "[" << log_level_str[(int)log_level] << "] " << msg << endl;
}

int ConnectTo(const std::string &ip_addr, in_port_t port) {
	sockaddr_in in4;

	memset(&in4, 0, sizeof(in4));
	if (inet_pton(AF_INET, ip_addr.c_str(), &in4.sin_addr) != 1) {
		return -1;
	}

	in4.sin_family = AF_INET;
	in4.sin_port = htons(port);

	int fd = socket(AF_INET , SOCK_STREAM , 0);
	if (fd < 0) {
		return fd;
	}

	if (connect(fd, reinterpret_cast<sockaddr*>(&in4), sizeof(in4)) != 0) {
		close(fd);
		return -1;
	}

	return fd;
}

void WaitCount::Dec() {
	lock_guard<mutex> lock(mut_);
	count_--;
	if (count_ == 0) {
		cond_.notify_one();
	}
	if (count_ < 0) {
		throw "too many decrements";
	}
}

bool WaitCount::Wait() {
	unique_lock<mutex> lock(mut_);
	return cond_.wait_for(lock, wait_time_, [this]{return count_ == 0;});
}

void Environment::TearDown() {
	EventLoop::DestroyAll();
}

