#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include <curl/curl.h>
#include <b64/b64.h>
#include <mylist/mylist.h>
#include "cashwebuni.h"

#define QUERY_LEN (71+strlen(QUERY_DATA_TAG)+strlen(QUERY_TXID_TAG))
#define TXID_QUERY_LEN (12+TXID_CHARS)
#define HEADER_BUF_SZ 35
#define QUERY_DATA_TAG "data"
#define RESPONSE_DATA_TAG "\""QUERY_DATA_TAG"\":\""
#define QUERY_TXID_TAG "txid"
#define RESPONSE_TXID_TAG "\""QUERY_TXID_TAG"\":\""

#define BITDB_API_VER 3
#define IS_BITDB_REQUEST_LIMIT 1

// gets the file at the specified txid and writes to specified file descriptor
// if foundHandler specified, will call to indicate if file is found
int getFile(const char *txid, const char *bitdbNode, int fd, void (*foundHandler) (int, int));

#endif
