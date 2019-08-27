CC = gcc
AM_CFLAGS = -g -Wall -O0 -fPIC
AM_LDFLAGS = -g

lib_LIBRARIES = libcashgettools.a
include_HEADERS = cashwebuni.h cashgettools.h
bin_PROGRAMS = cashget

if WITH_CASHSERVER
bin_PROGRAMS += cashserver
endif

if WITH_CASHSEND
lib_LIBRARIES += libcashsendtools.a
include_HEADERS += cashsendtools.h
bin_PROGRAMS += cashsend
endif

libmylist_sources = mylist/mylist.c
libb64encode_sources = b64/encode.c
libbitcoinrpc_sources = libbitcoinrpc/bitcoinrpc_err.c libbitcoinrpc/bitcoinrpc_cl.c libbitcoinrpc/bitcoinrpc.c libbitcoinrpc/bitcoinrpc_method.c libbitcoinrpc/bitcoinrpc_global.c libbitcoinrpc/bitcoinrpc_resp.c

EXTRA_DIST = cashwebutils.h mylist/mylist.h b64/b64.h libbitcoinrpc/*.h

libcashgettools_a_SOURCES = cashgettools.c cashwebutils.c $(libmylist_sources) $(libb64encode_sources)
libcashsendtools_a_SOURCES = cashsendtools.c cashwebutils.c $(libbitcoinrpc_sources)

cashget_SOURCES = cashget.c
cashserver_SOURCES = cashserver.c
cashsend_SOURCES = cashsend.c

LDADD = libcashgettools.a
cashsend_LDADD = libcashsendtools.a
