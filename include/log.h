/*
 * log.h
 *
 *  Created on: Feb 6, 2020
 *      Author: tomer
 */

#ifndef LIB_LOG_H_
#define LIB_LOG_H_

#include <string>
#include <sstream>
#include <memory>
#include <chrono>

namespace ael {

namespace log {

enum class LogLevel {
	Trace = 	0,
	Debug = 	1,
	Info = 		2,
	Warn = 		3,
	Error = 	4,
	Critical =  5,
	None = 		6
};

class Sink {
public:
	virtual ~Sink() {}

	virtual void Log(LogLevel log_level, const std::string &msg) = 0;

	static std::unique_ptr<Sink> sink_;
	static LogLevel log_level_;
	static std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;

protected:
	Sink() {}
};



#define LOG_LEVEL(log_level, msg) if (log::Sink::sink_ && log_level >= log::Sink::log_level_) { 								\
	std::ostringstream oss;																										\
	oss << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - log::Sink::start_time_).count() << "ms] " << __FILE__  << ":" << __LINE__ << " " << __FUNCTION__ << "(...) - " << msg;	\
	log::Sink::sink_->Log(log_level, oss.str());																				\
}																																\

#define LOG_TRACE(msg) LOG_LEVEL(log::LogLevel::Trace, msg)
#define LOG_DEBUG(msg) LOG_LEVEL(log::LogLevel::Debug, msg)
#define LOG_INFO(msg) LOG_LEVEL(log::LogLevel::Info, msg)
#define LOG_WARN(msg) LOG_LEVEL(log::LogLevel::Warn, msg)
#define LOG_ERROR(msg) LOG_LEVEL(log::LogLevel::Error, msg)
#define LOG_CRITICAL(msg) LOG_LEVEL(log::LogLevel::Crticial, msg)

}

}


#endif /* LIB_LOG_H_ */
