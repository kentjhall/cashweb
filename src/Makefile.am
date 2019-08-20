CC = gcc
AM_CFLAGS = -g -Wall -fPIC
AM_LDFLAGS = -g -L$(srcdir)

noinst_LIBRARIES = libmylist.a libb64encode.a libbitcoinrpc.a

libmylist_a_SOURCES = mylist/mylist.c
libb64encode_a_SOURCES = b64/encode.c
libbitcoinrpc_a_SOURCES = libbitcoinrpc/bitcoinrpc_err.c libbitcoinrpc/bitcoinrpc_cl.c libbitcoinrpc/bitcoinrpc.c libbitcoinrpc/bitcoinrpc_method.c libbitcoinrpc/bitcoinrpc_global.c libbitcoinrpc/bitcoinrpc_resp.c

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

EXTRA_DIST = cashwebutils.h mylist/mylist.h b64/b64.h libbitcoinrpc/*.h

libcashgettools_la_SOURCES = cashgettools.c cashwebutils.c
libcashsendtools_la_SOURCES = cashsendtools.c cashwebutils.c

libcashgettools_la_LIBADD = -lmylist -lb64encode
libcashsendtools_la_LIBADD = -lbitcoinrpc

cashget_SOURCES = cashget.c
cashserver_SOURCES = cashserver.c
cashsend_SOURCES = cashsend.c

LDADD = libcashgettools.la
cashsend_LDADD = libcashsendtools.la
