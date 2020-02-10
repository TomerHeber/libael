lib_LTLIBRARIES = libael.la

libael_la_SOURCES = event_loop.cc event.cc config.cc log.cc data_view.cc

otherincludedir = $(includedir)/ael
otherinclude_HEADERS = 	\
					$(top_srcdir)/include/event_loop.h \
				 	$(top_srcdir)/include/event.h \
				 	$(top_srcdir)/include/log.h \
				 	$(top_srcdir)/include/config.h \
				 	$(top_srcdir)/include/stream_listener.h \
				 	$(top_srcdir)/include/stream_buffer.h \
				 	$(top_srcdir)/include/data_view.h

if EPOLL_BACKEND
libael_la_SOURCES += linux/epoll.cc linux/stream_listener.cc linux/stream_buffer.cc		   
endif
 
AM_CXXFLAGS = -std=c++14 -Wall -Wpedantic -Wextra -I$(top_srcdir)/include