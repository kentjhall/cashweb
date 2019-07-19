#ifndef __CASHWEBUTILS_H__
#define __CASHWEBUTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/*
 * reports file size of file at given descriptor
 */
static inline long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

/*
 * struct/functions for dynamically sized heap-allocated memory
 */
struct DynamicMemory {
	char *data;
	size_t size;
};

static inline void initDynamicMemory(struct DynamicMemory *dm) {
	dm->data = NULL;
	dm->size = 0;
}

static inline void resizeDynamicMemory(struct DynamicMemory *dm, size_t newSize) {
	char *newDataAlloc;
	// sets data pointer to NULL on failure; this should be error-checked on use
	if ((newDataAlloc = realloc(dm->data, newSize)) == NULL) { perror("realloc failed"); free(dm->data); dm->data = NULL; return; }
	dm->data = newDataAlloc;
	dm->size = newSize;
}

static inline void freeDynamicMemory(struct DynamicMemory *dm) {
	if (dm->data) { free(dm->data); }
	initDynamicMemory(dm);
}

/*
 * converts byte array of n bytes to hex str of len 2n, and writes to specified memory location
 * must ensure hexStr has sufficient memory allocated
 */
static inline void byteArrToHexStr(const char *byteArr, int n, char *hexStr) {
	for (int i=0; i<n; i++) {
		hexStr[i*2]   = "0123456789abcdef"[((uint8_t)byteArr[i]) >> 4];
		hexStr[i*2+1] = "0123456789abcdef"[((uint8_t)byteArr[i]) & 0x0F];
	}
	hexStr[n*2] = 0;
}

/*
 * converts hex str to byte array, accounting for possible suffix to omit, and writes to specified memory location
 * must ensure byteArr has sufficient memory allocated
 * returns number of bytes read, or -1 on failure
 */
static inline int hexStrToByteArr(const char *hexStr, int suffixLen, char *byteArr) {
	int hexDataLen = strlen(hexStr);
	if (hexDataLen % 2 != 0) { return -1; }

	char hexByte[2+1];
	hexByte[2] = 0;
	int bytesRead = 0;
	for (int i=0; i<hexDataLen-suffixLen; i+=2) { 
		strncpy(hexByte, hexStr+i, 2);
		byteArr[i/2] = (char)strtoul(hexByte, NULL, 16);
		++bytesRead;
	}

	return bytesRead;
}

#endif
