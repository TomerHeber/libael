lib_LTLIBRARIES = libael.la

libael_la_SOURCES = eventloop.cc event.cc config.cc log.cc

include_HEADERS = eventloop.h event.h async_io.h config.h

if EPOLL_BACKEND
libael_la_SOURCES += linux/epoll.cc linux/stream_listener.cc
include_HEADERS += linux/stream_listener.h
endif
 
AM_CXXFLAGS = -std=c++14 -Wall -Wpedantic -Wextra