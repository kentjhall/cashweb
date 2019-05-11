#ifndef __CASHWEBTOOLS_H__
#define __CASHWEBTOOLS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include "b64/b64.h"
#include "cashwebtools.h"

#define TX_DATA_BYTES 219
#define TXID_BYTES 32

#define TX_DATA_CHARS TX_DATA_BYTES*2
#define TXID_CHARS TXID_BYTES*2
#define QUERY_LEN 134
#define HEADER_BUF_SZ 35
#define EXPECTED_RESPONSE_HEAD_U "{\"u\":[{\"data\":\""
#define EXPECTED_RESPONSE_HEAD_C "{\"u\":[],\"c\":[{\"data\":\""

#define BITDB_API_VER 3
#define IS_BITDB_REQUEST_LIMIT 1

extern char *bitdbNode;

void fetchFileWrite(char *txidStart, int fd);

#endif
