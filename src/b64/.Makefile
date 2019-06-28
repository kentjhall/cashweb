
CFLAGS = -std=c99 -Wall
TEST_CFLAGS = -Ideps
CUSTOM_MALLOC_FLAGS = -Db64_malloc=custom_malloc -Db64_realloc=custom_realloc

ifeq ($(USE_CUSTOM_MALLOC), true)
CFLAGS += $(CUSTOM_MALLOC_FLAGS)
endif

../libb64encode.a: encode.o
	ar rcs ../libb64encode.a encode.o

encode.o: encode.c b64.h

clean:
	rm -f *.o *.a ../*.a

.PHONY: default clean
