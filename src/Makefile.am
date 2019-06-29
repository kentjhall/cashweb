CC = gcc
AM_CFLAGS = -g -Wall
AM_LDFLAGS = -g 

noinst_LIBRARIES = libmylist.a libb64encode.a
libmylist_a_SOURCES = mylist/mylist.c mylist/mylist.h
libb64encode_a_SOURCES = b64/encode.c b64/b64.h

bin_PROGRAMS = cashserver cashget cashsend
cashserver_SOURCES = cashserver.c cashgettools.c cashgettools.h
cashget_SOURCES = cashget.c cashgettools.c cashgettools.h
cashsend_SOURCES = cashsend.c cashsendtools.c cashsendtools.h

cashserver_LDADD = libmylist.a libb64encode.a
cashget_LDADD = libmylist.a libb64encode.a
