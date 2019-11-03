CC = 
AM_CFLAGS = -I$(srcdir)/jansson -I$(srcdir)/jansson/src
if !WITH_EMSCRIPTEN
CC += gcc
AM_CFLAGS += -g -Wall -O3 -fPIC 
else
CC += emcc
AM_CFLAGS += --preload-file $(srcdir)/../data/CW_mimetypes@/data/CW_mimetypes -s WASM=1 -s FETCH=1 -s ASYNCIFY -s 'ASYNCIFY_IMPORTS=["jsFetch"]' -s ASSERTIONS=1 -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap"]' -I$(srcdir)/jansson -I$(srcdir)/jansson/src
endif
AM_LDFLAGS = -g \
	-no-undefined \
	-export-symbols-regex '^json_'

include_HEADERS = cashwebuni.h cashgettools.h

if !WITH_EMSCRIPTEN

lib_LIBRARIES = libcashgettools.a
bin_PROGRAMS = cashget

if WITH_CASHSERVER
bin_PROGRAMS += cashserver
endif

if WITH_CASHSEND
lib_LIBRARIES += libcashsendtools.a
include_HEADERS += cashsendtools.h
bin_PROGRAMS += cashsend
endif

else

bin_PROGRAMS = cashgettools_wasm.js
cashgettools_wasm_js_SOURCES = cashgettools_wasm.c
CLEANFILES = *.a *.wasm *.wast *.js *.data

endif

libmylist_sources = mylist/mylist.c
libb64encode_sources = b64/encode.c
libbitcoinrpc_sources = libbitcoinrpc/bitcoinrpc_err.c libbitcoinrpc/bitcoinrpc_cl.c libbitcoinrpc/bitcoinrpc.c libbitcoinrpc/bitcoinrpc_method.c libbitcoinrpc/bitcoinrpc_global.c libbitcoinrpc/bitcoinrpc_resp.c
libjansson_sources = \
	jansson/src/dump.c \
	jansson/src/error.c \
	jansson/src/hashtable.c \
	jansson/src/hashtable_seed.c \
	jansson/src/load.c \
	jansson/src/memory.c \
	jansson/src/pack_unpack.c \
	jansson/src/strbuffer.c \
	jansson/src/strconv.c \
	jansson/src/utf.c \
	jansson/src/value.c

EXTRA_DIST = cashwebutils.h cashfetchutils.h cashfetchhttputils.h mylist/mylist.h b64/b64.h libbitcoinrpc/*.h jansson/src/*.h

libcashgettools_a_SOURCES = cashgettools.c cashwebutils.c $(libmylist_sources) $(libb64encode_sources) $(libjansson_sources)
if WITH_MONGODB
libcashgettools_a_SOURCES += cashfetchutils.c
else
libcashgettools_a_SOURCES += cashfetchutils_nomongo.c
endif
libcashsendtools_a_SOURCES = cashsendtools.c cashwebutils.c $(libbitcoinrpc_sources) $(libjansson_sources)

cashget_SOURCES = cashget.c
cashserver_SOURCES = cashserver.c
cashsend_SOURCES = cashsend.c

LDADD = libcashgettools.a
cashsend_LDADD = libcashsendtools.a
