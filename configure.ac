AC_INIT([libael], [0.1])

LT_INIT

AC_CONFIG_SRCDIR([lib/eventloop.cc])

AC_CONFIG_MACRO_DIRS([m4])

AC_PROG_CXX

AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	tests/Makefile
])

AC_CANONICAL_HOST

build_linux=no

case "${host_os}" in
    linux*)
        build_linux=yes
        ;;  
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

AX_HAVE_EPOLL(
	[AX_CONFIG_FEATURE_ENABLE(epoll)],
	[AX_CONFIG_FEATURE_DISABLE(epoll)]
)
AX_CONFIG_FEATURE(
	[epoll], [This platform supports epoll(7)],
	[HAVE_EPOLL], [This platform supports epoll(7).],
	[enable_epoll="yes"], [enable_epoll="no"]
)

AM_INIT_AUTOMAKE([subdir-objects])

AM_CONDITIONAL([EPOLL_BACKEND], [test "x$enable_epoll" = "xyes"])
AM_CONDITIONAL([LINUX], [test "x$build_linux" = "xyes"])

AC_OUTPUT