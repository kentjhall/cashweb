#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include <curl/curl.h>
#include "b64/b64.h"
#include "cashwebuni.h"

#define QUERY_LEN 134
#define HEADER_BUF_SZ 35
#define RESPONSE_DATA_TAG "data"
#define RESPONSE_DATA_TAG_QUERY "\""RESPONSE_DATA_TAG"\":\""

#define BITDB_API_VER 3
#define IS_BITDB_REQUEST_LIMIT 1

extern char *bitdbNode;

// gets the file at the specified txid and writes to specified file descriptor
void getWriteFile(char *txidStart, int fd);

#endif
