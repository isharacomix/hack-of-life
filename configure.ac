AC_INIT([HackOfLife], [1.0.2], [ishara@isharacomix.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])

AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
AC_PROG_INSTALL

AC_HEADER_TIME

AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_LIB([curses], [initscr], ,
   AC_MSG_ERROR([ *** Can't find curses headers.]))

AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h malloc.h netdb.h netinet/in.h stdlib.h stdio.h string.h sys/socket.h unistd.h fcntl.h errno.h])

AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_ARG_WITH(ipv6,[  --without-ipv6          don't use IPv6 ] )
if test "x$with_ipv6" == "xno"; then
    with_ipv6=no
else
    with_ipv6=yes
    AC_DEFINE(USE_IPV6, 1, [Defines whether or not to use IPv6 support])
fi

AC_OUTPUT
echo
echo "Now run 'make' to compile the program."
echo

