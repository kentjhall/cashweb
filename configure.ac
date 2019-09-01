#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([cashwebtools], [0])
AC_CONFIG_SRCDIR([src/cashget.c])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AC_ARG_WITH([cashserver],
	[AS_HELP_STRING([--with-cashserver], [specify whether or not to build cashserver binary, default=yes] @@)],
	[],
	[with_cashserver=yes])
AC_ARG_WITH([cashsend],
	[AS_HELP_STRING([--with-cashsend], [specify whether or not to build cashsendtools library + cashsend binary, default=yes] @@)],
	[],
	[with_cashsend=yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
AC_LTDL_DLLIB
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Allows datadir to be specified
dnl Defined in the file ac_define_dir.m4 (needs to be listed in acinclude.m4)
AC_DEFINE_DIR([DATADIR], [datadir], [Directory for data files.])

# Checks for libraries/corresponding header files.

CFLAGS="$(pkg-config --cflags libmongoc-1.0) ${CFLAGS}"
LDFLAGS="$(pkg-config --libs libmongoc-1.0) ${LDFLAGS}"

AC_SEARCH_LIBS([mongoc_init], [mongoc-1.0], [], [
  AC_MSG_ERROR([unable to find mongoc; please install this library with your package manager, and try again])
])
AC_SEARCH_LIBS([bson_destroy], [bson-1.0], [], [
  AC_MSG_ERROR([unable to find bson; please install this library with your package manager (should come included with mongoc), and try again])
])
AC_CHECK_HEADER([mongoc.h], [], [
  AC_MSG_ERROR([unable to find header mongoc.h, please verify mongoc library installation and try again])
])
AC_SEARCH_LIBS([curl_easy_init], [curl], [], [
  AC_MSG_ERROR([unable to find curl; please install this library with your package manager, and try again])
])
AC_CHECK_HEADER([curl/curl.h], [], [
  AC_MSG_ERROR([unable to find header curl/curl.h, please verify curl library installation and try again])
])
AC_SEARCH_LIBS([json_object], [jansson], [], [
  AC_MSG_ERROR([unable to find jansson, which is necessary for cashsend; please install library with your package manager and try again, or if cashsend is not needed, try again with flag --without-cashsend])
])
AC_CHECK_HEADER([jansson.h], [], [
  AC_MSG_ERROR([unable to find header jansson.h; please verify jansson library installation and try again])
])

AS_IF([test "x$with_cashserver" != xno],
	[
	AC_SEARCH_LIBS([MHD_start_daemon], [microhttpd], [], [
	  AC_MSG_ERROR([unable to find libmicrohttpd, which is necessary for cashserver; please install this library with your package manager and try again, or if cashserver is not needed, try again with flag --without-cashserver])
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
	AC_SEARCH_LIBS([uuid_generate_random], [uuid], [], [
	  AC_MSG_ERROR([unable to find uuid, which is necessary for cashsend; please install library with your package manager and try again, or if cashsend is not needed, try again with flag --without-cashsend])
	])
	AC_CHECK_HEADER([uuid/uuid.h], [], [
	  AC_MSG_ERROR([unable to find header jansson.h; please verify jansson library installation and try again])
	])
	AM_CONDITIONAL(WITH_CASHSEND, true)
	], [
	AM_CONDITIONAL(WITH_CASHSEND, false)
	])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h stdlib.h stdint.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero inet_ntoa memset strdup strrchr strstr strtoul])

AC_OUTPUT(Makefile src/Makefile data/Makefile)
