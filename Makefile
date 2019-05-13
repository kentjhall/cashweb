CC = gcc
CFLAGS = -g -Wall 
LDFLAGS = -g
LDLIBS = -lcurl
VPATH = ./b64

cashsend: cashsend.o cashsendtools.o
cashget: cashget.o cashgettools.o encode.o 

cashsend.o: cashsend.c cashsendtools.h cashwebuni.h
cashsendtools.o: cashsendtools.c cashsendtools.h cashwebuni.h
cashget.o: cashget.c cashgettools.h cashwebuni.h
cashgettools.o: cashgettools.c cashgettools.h cashwebuni.h b64.h
encode.o: encode.c b64.h

.PHONY: clean
clean:
	rm -rf cashsend cashget *.o *.dSYM

.PHONY: all
all: clean cashsend cashget
