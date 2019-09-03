#ifndef __CASHWEBUTILS_H__
#define __CASHWEBUTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <jansson.h>

#define FILE_DATA_BUF 1024

/*
 * struct/functions for dynamically sized heap-allocated memory
 */
struct DynamicMemory {
	char *data;
	size_t size;
};

void initDynamicMemory(struct DynamicMemory *dm);

void resizeDynamicMemory(struct DynamicMemory *dm, size_t newSize);

void freeDynamicMemory(struct DynamicMemory *dm);

/*
 * reports file size of file at given descriptor; returns -1 on failure
 */
long fileSize(int fd);

/*
 * safely read entire line from fp using struct DynamicMemory, stripping newline character if present
 * passed struct DynamicMemory must be initialized; lineBufStart is passed for initial sizing
 * returns READLINE_YES on success, READLINE_NO on fgets() stopping (so file error/EOF still needs to be checked), or READLINE_ERR on unrelated error
 */
#define READLINE_OK 0
#define READLINE_NO 1
#define READLINE_ERR 2
int safeReadLine(struct DynamicMemory *line, size_t lineBufStart, FILE *fp);

/*
 * reads data from source and writes to dest
 * returns COPY_OK on success or COPY_READ_ERR/COPY_WRITE_ERR as appropriate
 */
#define COPY_OK 0
#define COPY_READ_ERR 1
#define COPY_WRITE_ERR 2
int copyStreamData(FILE *dest, FILE *source);

/*
 * reads data from source and writes to dest file descriptor
 * returns COPY_OK on success or COPY_READ_ERR/COPY_WRITE_ERR as appropriate
 */
int copyStreamDataFildes(int dest, FILE *source);

/*
 * converts byte array of n bytes to hex str of len 2n, and writes to specified memory location
 * must ensure hexStr has sufficient memory allocated (2n + 1); always null-terminates
 */
void byteArrToHexStr(const char *byteArr, int n, char *hexStr);

/*
 * converts hex str to byte array, accounting for possible suffix to omit, and writes to specified memory location
 * must ensure byteArr has sufficient memory allocated
 * returns number of bytes read
 */
int hexStrToByteArr(const char *hexStr, int suffixLen, char *byteArr);

/*
 * puts int to network byte order (big-endian) byte array, written to passed memory location
 * must ensure byteArr has sizeof(uint32_t) bytes allocated
 */
void int32ToNetByteArr(uint32_t uint, unsigned char *byteArr);

/*
 * puts int to network byte order (big-endian) byte array, written to passed memory location
 * must ensure byteArr has sizeof(uint16_t) bytes allocated
 */
void int16ToNetByteArr(uint16_t uint, unsigned char *byteArr);

/*
 * puts int to network byte order (big-endian) byte array, written to passed memory location
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes), and that byteArr has sufficient space
 * supports 2 and 4 byte integers
 */
bool intToNetByteArr(void *uintPtr, int numBytes, unsigned char *byteArr);

/*
 * puts int to network byte order (big-endian) and converts to hex string, written to passed memory location
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes), and that hexStr has sufficient space
 * supports 2 and 4 byte integers
 * must ensure hexStr has sufficient memory allocated (2n + 1); always null-terminates
 */
bool intToNetHexStr(void *uintPtr, int numBytes, char *hexStr);

/*
 * converts byte array in network byte order (big-endian) to host byte order integer
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes)
 * supports 2 and 4 byte integers
 */
bool netByteArrToInt(const char *byteData, int numBytes, void *uintPtr);

/* 
 * resolves array of bytes into a network byte order (big-endian) unsigned integer,
 * and then converts to host byte order
 * must make sure void* is appropriate unsigned integer type (uint16_t or uint32_t),
 * and passed hex must be appropriate length
 */
bool netHexStrToInt(const char *hex, int numBytes, void *uintPtr);

#endif
