CC = gcc
AM_CFLAGS = -g -Wall
AM_LDFLAGS = -g

noinst_LIBRARIES = libmylist.a libb64encode.a libbitcoinrpc.a

libmylist_a_SOURCES = mylist/mylist.c mylist/mylist.h
libb64encode_a_SOURCES = b64/encode.c b64/b64.h
libbitcoinrpc_a_SOURCES = libbitcoinrpc/bitcoinrpc_err.c libbitcoinrpc/bitcoinrpc_cl.c libbitcoinrpc/bitcoinrpc.c libbitcoinrpc/bitcoinrpc_method.c libbitcoinrpc/bitcoinrpc_global.c libbitcoinrpc/bitcoinrpc_resp.c libbitcoinrpc/*.h

lib_LTLIBRARIES = libcashgettools.la
include_HEADERS = cashwebuni.h cashgettools.h
bin_PROGRAMS = cashget

if WITH_CASHSERVER
bin_PROGRAMS += cashserver
endif

if WITH_CASHSEND
lib_LTLIBRARIES += libcashsendtools.la
include_HEADERS += cashsendtools.h
bin_PROGRAMS += cashsend
endif

EXTRA_DIST = cashwebutils.h

libcashgettools_la_SOURCES = cashgettools.c cashwebutils.c
libcashsendtools_la_SOURCES = cashsendtools.c cashwebutils.c

libcashgettools_la_LDADD = libmylist.a libb64encode.a
libcashsendtools_la_LDADD = libbitcoinrpc.a

cashget_SOURCES = cashget.c
cashserver_SOURCES = cashserver.c
cashsend_SOURCES = cashsend.c

LDADD = libcashgettools.la
cashsend_LDADD = libcashsendtools.la
