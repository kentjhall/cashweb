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
 * safely read entire line from fp using struct DynamicMemory, stripping newline character if present
 * passed struct DynamicMemory must be initialized; lineBufStart is passed for initial sizing
 * returns READLINE_YES on success, READLINE_NO on fgets() stopping (so file error/EOF still needs to be checked), or READLINE_ERR on unrelated error
 */
 #define READLINE_OK 0
 #define READLINE_NO 1
 #define READLINE_ERR 2
static int safeReadLine(struct DynamicMemory *line, size_t lineBufStart, FILE *fp) {
	resizeDynamicMemory(line, lineBufStart);
	if (line->data == NULL) { return READLINE_ERR; }
	bzero(line->data, line->size);

	int lineLen;
	char lastChar;
	int offset = 0;
	while (fgets(line->data+offset, line->size-1-offset, fp) != NULL) {
		lineLen = strlen(line->data);
		lastChar = line->data[lineLen-1];
		if (lastChar != '\n' && !feof(fp)) {
			offset = lineLen;
			resizeDynamicMemory(line, line->size*2);
			if (line->data == NULL) { return READLINE_ERR; }
			bzero(line->data+offset, line->size-offset);
			continue;
		}
		if (lastChar == '\n') { line->data[lineLen-1] = 0; }
		return READLINE_OK;
	}

	return READLINE_NO;
}

/*
 * reads data from source and writes to dest
 * returns true on success or false on system error
 */
#define FILE_DATA_BUF 1024
static inline bool copyStreamData(FILE *dest, FILE *source) {
	char buf[FILE_DATA_BUF];
	int n;
	while ((n = fread(buf, 1, FILE_DATA_BUF, source)) > 0) {
		if (fwrite(buf, 1, n, dest) < n) { perror("fwrite() failed"); return false; }
	}
	if (ferror(source)) { perror("fread() failed"); return false; }
	return true;
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
