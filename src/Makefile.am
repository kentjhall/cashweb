CC = gcc
AM_CFLAGS = -g -Wall
AM_LDFLAGS = -g

noinst_LIBRARIES = libmylist.a libb64encode.a libbitcoinrpc.a

libmylist_a_SOURCES = mylist/mylist.c mylist/mylist.h
libb64encode_a_SOURCES = b64/encode.c b64/b64.h
libbitcoinrpc_a_SOURCES = libbitcoinrpc/bitcoinrpc_err.c libbitcoinrpc/bitcoinrpc_cl.c libbitcoinrpc/bitcoinrpc.c libbitcoinrpc/bitcoinrpc_method.c libbitcoinrpc/bitcoinrpc_global.c libbitcoinrpc/bitcoinrpc_resp.c libbitcoinrpc/bitcoinrpc.h

bin_PROGRAMS = cashget
if WITH_CASHSERVER
bin_PROGRAMS += cashserver
endif
if WITH_CASHSEND
bin_PROGRAMS += cashsend
endif

cashget_SOURCES = cashget.c cashgettools.c cashgettools.h cashwebuni.h
cashserver_SOURCES = cashserver.c cashgettools.c cashgettools.h cashwebuni.h
cashsend_SOURCES = cashsend.c cashsendtools.c cashsendtools.h cashwebuni.h

LDADD = libmylist.a libb64encode.a 
cashsend_LDADD = libbitcoinrpc.a
