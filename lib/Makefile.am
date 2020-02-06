lib_LTLIBRARIES = libael.la

libael_la_SOURCES = eventloop.cc event.cc config.cc log.cc

include_HEADERS = 	\
					$(top_srcdir)/include/eventloop.h \
				 	$(top_srcdir)/include/event.h \
				 	$(top_srcdir)/include/log.h \
				 	$(top_srcdir)/include/config.h

if EPOLL_BACKEND
libael_la_SOURCES += linux/epoll.cc linux/stream_listener.cc
include_HEADERS += \
				   $(top_srcdir)/include/linux/stream_listener.h
endif
 
AM_CXXFLAGS = -std=c++14 -Wall -Wpedantic -Wextra -I$(top_srcdir)/include