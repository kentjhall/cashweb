CC = gcc
AM_CFLAGS = -g -Wall
AM_LDFLAGS = -g 

noinst_LIBRARIES = libmylist.a libb64encode.a
libmylist_a_SOURCES = mylist/mylist.c mylist/mylist.h
libb64encode_a_SOURCES = b64/encode.c b64/b64.h

bin_PROGRAMS = cashget
if WITH_CASHSERVER
bin_PROGRAMS += cashserver
endif
if WITH_CASHSEND
bin_PROGRAMS += cashsend
endif

cashget_SOURCES = cashget.c cashgettools.c cashgettools.h
cashserver_SOURCES = cashserver.c cashgettools.c cashgettools.h
cashsend_SOURCES = cashsend.c cashsendtools.c cashsendtools.h

LDADD = libmylist.a libb64encode.a
cashsend_LDADD = 
