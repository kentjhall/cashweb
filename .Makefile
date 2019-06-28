CC = gcc
CFLAGS = -g -Wall -I./src -I./libs
LDFLAGS =  -g -L./libs
LDLIBS = -lcurl -lmicrohttpd -lmylist -lb64encode
VPATH = src

.PHONY: default
default: libs cashsend cashget cashserver

cashserver: cashserver.o cashgettools.o
cashget: cashget.o cashgettools.o
cashsend: cashsend.o cashsendtools.o 

cashserver.o: cashserver.c cashgettools.h cashwebuni.h
cashget.o: cashget.c cashgettools.h cashwebuni.h
cashsend.o: cashsend.c cashsendtools.h cashwebuni.h

cashgettools.o: cashgettools.c cashgettools.h cashwebuni.h
cashsendtools.o: cashsendtools.c cashsendtools.h cashwebuni.h

.PHONY: clean
clean:
	rm -rf cashsend cashget cashserver *.o *.a *.dSYM
	find libs -type d -depth 1 -exec $(MAKE) clean -C {} \;

.PHONY: all
all: clean libs cashsend cashget cashserver

.PHONY: libs
libs:
	find libs -type d -depth 1 -exec $(MAKE) -C {} \;
