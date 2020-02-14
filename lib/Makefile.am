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
libael_la_SOURCES += linux/epoll.cc linux/stream_listener.cc linux/stream_buffer.cc linux/tcp_stream_buffer_filter.cc		   
endif

if OPENSSL
lib_LTLIBRARIES += libael_openssl.la
otherinclude_HEADERS += $(top_srcdir)/include/ssl_stream_buffer_filter.h
libael_openssl_la_SOURCES = openssl/ssl_stream_buffer_filter.cc
libael_openssl_la_LIBADD =  $(OPENSSL_LIBS) 
libael_openssl_la_LDFLAGS = $(OPENSSL_LDFLAGS)
libael_openssl_la_CPPFLAGS = -I$(OPENSSL_INCLUDES)
endif
 
AM_CXXFLAGS = -std=c++14 -Wall -Wpedantic -Wextra -I$(top_srcdir)/include