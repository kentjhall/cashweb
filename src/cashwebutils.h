#ifndef __CASHWEBUTILS_H__
#define __CASHWEBUTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/*
 * reports file size of file at given descriptor; returns -1 on failure
 */
static inline long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { perror("fstat() failed"); return -1; }
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
	while ((n = fread(buf, 1, sizeof(buf), source)) > 0) {
		if (fwrite(buf, 1, n, dest) < n) { perror("fwrite() failed"); return false; }
	}
	if (ferror(source)) { perror("fread() failed"); return false; }
	return true;
}

/*
 * converts byte array of n bytes to hex str of len 2n, and writes to specified memory location
 * must ensure hexStr has sufficient memory allocated (2n + 1); always null-terminates
 */
static inline void byteArrToHexStr(const char *byteArr, int n, char *hexStr) {
	for (int i=0; i<n; i++) {
		hexStr[i*2] = "0123456789abcdef"[((uint8_t)byteArr[i]) >> 4];
		hexStr[i*2+1] = "0123456789abcdef"[((uint8_t)byteArr[i]) & 0x0F];
	}
	hexStr[n*2] = 0;
}

/*
 * converts hex str to byte array, accounting for possible suffix to omit, and writes to specified memory location
 * must ensure byteArr has sufficient memory allocated
 * returns number of bytes read
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

/*
 * puts int to network byte order (big-endian) byte array, written to passed memory location
 * must ensure byteArr has sizeof(uint32_t) bytes allocated
 */
static inline void int32ToNetByteArr(uint32_t uint, unsigned char *byteArr) {
	uint32_t netUInt = htonl(uint);
	for (int i=0; i<sizeof(netUInt); i++) {
		byteArr[i] = (netUInt >> i*8) & 0xFF;
	}
}

/*
 * puts int to network byte order (big-endian) byte array, written to passed memory location
 * must ensure byteArr has sizeof(uint16_t) bytes allocated
 */
static inline void int16ToNetByteArr(uint16_t uint, unsigned char *byteArr) {
	uint16_t netUInt = htons(uint);
	for (int i=0; i<sizeof(netUInt); i++) {
		byteArr[i] = (netUInt >> i*8) & 0xFF;
	}
}

/*
 * puts int to network byte order (big-endian) byte array, written to passed memory location
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes), and that byteArr has sufficient space
 * supports 2 and 4 byte integers
 */
static inline bool intToNetByteArr(void *uintPtr, int numBytes, unsigned char *byteArr) {
	switch (numBytes) {
		case sizeof(uint16_t):
		{
			uint16_t uint16 = *(uint16_t *)uintPtr;
			int16ToNetByteArr(uint16, byteArr);
			break;
		}
		case sizeof(uint32_t):
		{
			uint32_t uint32 = *(uint32_t *)uintPtr;
			int32ToNetByteArr(uint32, byteArr);
			break;
		}
		default:
			fprintf(stderr, "invalid byte count specified for intToNetByteArr, int must be 2 or 4 bytes; problem with cashwebtools\n");
			return false;
	}
	return true;
}

/*
 * puts int to network byte order (big-endian) and converts to hex string, written to passed memory location
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes), and that hexStr has sufficient space
 * supports 2 and 4 byte integers
 * must ensure hexStr has sufficient memory allocated (2n + 1); always null-terminates
 */
static inline bool intToNetHexStr(void *uintPtr, int numBytes, char *hexStr) {
	unsigned char bytes[numBytes];
		
	if (!intToNetByteArr(uintPtr, numBytes, bytes)) { return false; }
	byteArrToHexStr((const char *)bytes, numBytes, hexStr);

	return true;
}

/*
 * converts byte array in network byte order (big-endian) to host byte order integer
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes)
 * supports 2 and 4 byte integers
 */
static inline bool netByteArrToInt(const char *byteData, int numBytes, void *uintPtr) {
	uint16_t uint16 = 0; uint32_t uint32 = 0;
	bool isShort = false;
	switch (numBytes) {
		case sizeof(uint16_t):
			isShort = true;
			break;
		case sizeof(uint32_t):
			break;
		default:
			fprintf(stderr, "unsupported number of bytes read for network integer value; probably problem with cashwebtools\n");
			return false;
	}

	for (int i=0; i<numBytes; i++) {
		if (isShort) { uint16 |= (uint16_t)byteData[i] << i*8; } else { uint32 |= (uint32_t)byteData[i] << i*8; }
	}
	if (isShort) { *(uint16_t *)uintPtr = ntohs(uint16); }
	else { *(uint32_t *)uintPtr = ntohl(uint32); }

	return true;
}

/* 
 * resolves array of bytes into a network byte order (big-endian) unsigned integer,
 * and then converts to host byte order
 * must make sure void* is appropriate unsigned integer type (uint16_t or uint32_t),
 * and passed hex must be appropriate length
 */
static inline bool netHexStrToInt(const char *hex, int numBytes, void *uintPtr) {
	char byteData[numBytes];
	if (numBytes != strlen(hex)/2 || hexStrToByteArr(hex, 0, byteData) < 0) { fprintf(stderr, "invalid hex data passed for network integer value; probably problem with cashwebtools\n"); return false; }	

	return netByteArrToInt(byteData, numBytes, uintPtr);
}

#endif
