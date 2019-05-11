CC = gcc
CFLAGS = -g -Wall 
LDFLAGS = -g
LDLIBS = -lcurl
VPATH = ./b64

cashget: cashget.o cashwebtools.o encode.o 

cashget.o: cashget.c cashwebtools.h

cashwebtools.o: cashwebtools.c cashwebtools.h b64.h

encode.o: encode.c b64.h

.PHONY: clean
clean:
	rm -rf cashget *.o *.dSYM

.PHONY: all
all: clean cashget
