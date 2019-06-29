#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([cashwebtools], [0])
AC_CONFIG_SRCDIR([src/cashsend.c])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
AC_SEARCH_LIBS([curl_easy_init], [curl], [], [
  AC_MSG_ERROR([unable to find libcurl, please install and try again])
])
AC_SEARCH_LIBS([MHD_start_daemon], [microhttpd], [], [
  AC_MSG_ERROR([unable to find libmicrohttpd, please install and try again])
])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h stdlib.h string.h unistd.h])
AC_CHECK_HEADER([curl/curl.h], [], [
  AC_MSG_ERROR([unable to find header curl/curl.h, please make sure it's installed])
])
AC_CHECK_HEADER([microhttpd.h], [], [
  AC_MSG_ERROR([unable to find header microhttpd.h, please make sure it's installed])
])

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
