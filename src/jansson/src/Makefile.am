EXTRA_DIST = jansson.def

include_HEADERS = jansson.h
nodist_include_HEADERS = jansson_config.h

lib_LIBRARIES = libjansson.a
libjansson_a_SOURCES = \
	dump.c \
	error.c \
	hashtable.c \
	hashtable.h \
	hashtable_seed.c \
	jansson_private.h \
	load.c \
	lookup3.h \
	memory.c \
	pack_unpack.c \
	strbuffer.c \
	strbuffer.h \
	strconv.c \
	utf.c \
	utf.h \
	value.c
libjansson_a_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex '^json_' \
	-version-info 15:1:11 \
	@JSON_BSYMBOLIC_LDFLAGS@
