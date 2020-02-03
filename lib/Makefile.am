lib_LTLIBRARIES = libael.la

libael_la_SOURCES = eventloop.cc

include_HEADERS = eventloop.h async_io.h

if EPOLL_BACKEND
libael_la_SOURCES += async_io/epoll.cc
include_HEADERS +=  async_io/epoll.h
endif
 
AM_CXXFLAGS = -Wall -Wpedantic -Wextra