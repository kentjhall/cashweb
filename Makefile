CC = gcc
CFLAGS = -g -Wall 
LDFLAGS =  -g 
LDLIBS = -lcurl -lmicrohttpd -lprimesieve
VPATH = ./b64 ./minilzo

cashserver: cashserver.o cashgettools.o encode.o
cashget: cashget.o cashgettools.o minilzo.o encode.o
cashsend: cashsend.o cashsendtools.o minilzo.o

cashserver.o: cashserver.c cashgettools.h cashwebuni.h minilzo.h
cashget.o: cashget.c cashgettools.h cashwebuni.h minilzo.h
cashsend.o: cashsend.c cashsendtools.h cashwebuni.h minilzo.h

cashgettools.o: cashgettools.c cashgettools.h b64.h cashwebuni.h minilzo.h
cashsendtools.o: cashsendtools.c cashsendtools.h cashwebuni.h minilzo.h

minilzo.o: minilzo.c minilzo.h
encode.o: encode.c b64.h

.PHONY: clean
clean:
	rm -rf cashsend cashget cashserver *.o *.dSYM

.PHONY: all
all: clean cashsend cashget cashserver
