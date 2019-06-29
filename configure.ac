#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([cashwebtools], [0])
AC_CONFIG_SRCDIR([src/cashsend.c])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AC_ARG_WITH([cashserver],
	[AS_HELP_STRING([--with-cashserver], [specify whether or not to build cashserver binary, default=yes] @@)],
	[],
	[with_cashserver=yes])
AC_ARG_WITH([cashsend],
	[AS_HELP_STRING([--with-cashsend], [specify whether or not to build cashsend binary, default=yes] @@)],
	[],
	[with_cashsend=yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries/corresponding header files.
AC_SEARCH_LIBS([curl_easy_init], [curl], [], [
  AC_MSG_ERROR([unable to find curl, please install library and try again])
])
AC_CHECK_HEADER([curl/curl.h], [], [
  AC_MSG_ERROR([unable to find header curl/curl.h, please verify curl library installation and try again])
])
AS_IF([test "x$with_cashserver" != xno],
	[
	AC_SEARCH_LIBS([MHD_start_daemon], [microhttpd], [], [
	  AC_MSG_ERROR([unable to find libmicrohttpd, which is necessary for cashserver; please install library and try again, or if not needed, try again with flag --without-cashserver])
	])
	AC_CHECK_HEADER([microhttpd.h], [], [
	  AC_MSG_ERROR([unable to find header microhttpd.h; please verify libmicrohttpd library installation and try again])
	])
	AM_CONDITIONAL(WITH_CASHSERVER, true)
	], [
	AM_CONDITIONAL(WITH_CASHSERVER, false)
	])
AS_IF([test "x$with_cashsend" != xno],
	[
	AC_SEARCH_LIBS([json_object], [jansson], [], [
	  AC_MSG_ERROR([unable to find jansson, which is necessary for cashsend; please install library and try again, or if not needed, try again with flag --without-cashsend])
	])
	AC_CHECK_HEADER([jansson.h], [], [
	  AC_MSG_ERROR([unable to find header jansson.h; please verify jansson library installation and try again])
	])
	AM_CONDITIONAL(WITH_CASHSEND, true)
	], [
	AM_CONDITIONAL(WITH_CASHSEND, false)
	])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([inet_ntoa memset strstr strtoul])

AC_OUTPUT(Makefile src/Makefile)
