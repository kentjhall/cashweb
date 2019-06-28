CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -g -L.
LDLIBS = -lmylist

../libmylist.a: mylist.o
	ar rcs ../libmylist.a mylist.o

mylist.o: mylist.c mylist.h 

.PHONY: clean
clean:
	rm -rf *.o *.a ../*.a

.PHONY: all
all: clean libmylist.a
