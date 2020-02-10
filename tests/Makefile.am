check_LTLIBRARIES = libgtest.la
libgtest_la_SOURCES = ../googletest/googletest/src/gtest-all.cc
libgtest_la_CPPFLAGS = -I$(top_srcdir)/googletest/googletest/include -I$(top_srcdir)/googletest/googletest
libgtest_la_LDFLAGS = -pthread

check_PROGRAMS = tcp_test execute_test data_view_test

tcp_test_SOURCES = tcp_test.cc helpers.cc
tcp_test_LDADD = libgtest.la ../lib/libael.la
tcp_test_LDFLAGS = -pthread
tcp_test_CPPFLAGS = -I$(top_srcdir)/googletest/googletest/include -I$(top_srcdir)/include

execute_test_SOURCES = execute_test.cc helpers.cc
execute_test_LDADD = libgtest.la ../lib/libael.la
execute_test_LDFLAGS = -pthread
execute_test_CPPFLAGS = -I$(top_srcdir)/googletest/googletest/include -I$(top_srcdir)/include

data_view_test_SOURCES = data_view_test.cc helpers.cc
data_view_test_LDADD = libgtest.la ../lib/libael.la
data_view_test_LDFLAGS = -pthread
data_view_test_CPPFLAGS = -I$(top_srcdir)/googletest/googletest/include -I$(top_srcdir)/include

TESTS = tcp_test execute_test data_view_test