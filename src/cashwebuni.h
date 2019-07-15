#ifndef __CASHWEBUNI_H__
#define __CASHWEBUNI_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <jansson.h>

#define CW_OK 0
typedef int CW_STATUS;

// #defines protocol version specific specifiers
#define CW_P_VER 0

#define CW_T_FILE 0
#define CW_T_DIR 1

struct cwFileMetadata {
	uint32_t length;
	uint32_t depth;
	uint16_t type;
	uint16_t pVer;
};

static inline void initCwFileMetadata(struct cwFileMetadata *md, uint16_t cwFType) {
	md->length = 0;
	md->depth = 0;
	md->type = cwFType;
	md->pVer = CW_P_VER;
}

#define CW_MD_BYTES(md) sizeof(((struct cwFileMetadata *)0)->md)
#define CW_METADATA_BYTES (CW_MD_BYTES(length)+\
			   CW_MD_BYTES(depth)+\
			   CW_MD_BYTES(type)+\
			   CW_MD_BYTES(pVer))

// #defines based on network rules
#define TX_RAW_DATA_BYTES 222
#define TX_DATA_BYTES (TX_RAW_DATA_BYTES-2)
#define TXID_BYTES 32

// #defines _CHARS for number of hex data chars
#define HEX_CHARS(bytes) (bytes*2)
#define CW_MD_CHARS(md) HEX_CHARS(CW_MD_BYTES(md))
#define CW_METADATA_CHARS HEX_CHARS(CW_METADATA_BYTES)
#define TX_RAW_DATA_CHARS HEX_CHARS(TX_RAW_DATA_BYTES)
#define TX_DATA_CHARS HEX_CHARS(TX_DATA_BYTES)
#define TXID_CHARS HEX_CHARS(TXID_BYTES)

/*
 * reports system error + custom message and exits
 */
static inline void die(char *e) { perror(e); exit(1); }

/*
 * reports file size of file at given descriptor
 */
static inline long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

/*
 * struct/functions for dynamically sized heap-allocated memory, pretty self-explanatory
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
	if ((dm->data = realloc(dm->data, newSize)) == NULL) { free(dm->data); die("realloc failed"); }
	dm->size = newSize;
}

static inline void freeDynamicMemory(struct DynamicMemory *dm) {
	if (dm->data) { free(dm->data); }
	initDynamicMemory(dm);
}

/*
 * converts byte array of n bytes to hex str of len 2n, and writes to specified memory loc
 */
static inline void byteArrToHexStr(const char *fileBytes, int n, char *hex) {
	for (int i=0; i<n; i++) {
		hex[i*2]   = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) >> 4];
		hex[i*2+1] = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) & 0x0F];
	}
	hex[n*2] = 0;
}

/*
 * converts hex str to byte array, accounting for possible suffix to omit, and writes to specified memory loc
 * returns number of bytes read, or -1 on failure
 */
static inline int hexStrToByteArr(const char *hexData, int suffixLen, char *byteData) {
	int hexDataLen = strlen(hexData);
	if (hexDataLen % 2 != 0) { return -1; }

	char hexByte[2+1];
	hexByte[2] = 0;
	int bytesRead = 0;
	for (int i=0; i<hexDataLen-suffixLen; i+=2) { 
		strncpy(hexByte, hexData+i, 2);
		byteData[i/2] = (char)strtoul(hexByte, NULL, 16);
		++bytesRead;
	}

	return bytesRead;
}

#endif
