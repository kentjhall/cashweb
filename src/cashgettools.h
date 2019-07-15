#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include <curl/curl.h>
#include <libmongoc/mongoc/mongoc.h>
#include <b64/b64.h>
#include <mylist/mylist.h>
#include "cashwebuni.h"

// #defines error codes
#define CWG_IN_DIR_NO 1
#define CWG_IS_DIR_NO 2
#define CWG_FETCH_NO 3
#define CWG_METADATA_NO 4
#define CWG_SYS_ERR 5
#define CWG_FETCH_ERR 6
#define CWG_WRITE_ERR 7
#define CWG_FILE_ERR 8
#define CWG_FILE_LEN_ERR 9
#define CWG_FILE_DEPTH_ERR 10

// #defines defaults
#define CWG_MAX_HEAP_ALLOC_MB_DEFAULT 1024

struct cwGetParams {
	char *bitdbNode;
	char *dirPath;
	FILE *saveDirFp;
	void (*foundHandler) (CW_STATUS, void *, int);
	void *foundHandleData;
	CW_STATUS foundSuppressErr;
};

static inline void initCwGetParams(struct cwGetParams *cgp, char *bitdbNode) {
	cgp->bitdbNode = bitdbNode;
	cgp->dirPath = NULL;
	cgp->saveDirFp = NULL;
	cgp->foundHandler = NULL;
	cgp->foundHandleData = NULL;
	cgp->foundSuppressErr = -1;
}

/*
 * returns generic error message by error code
 */
const char *cwgErrNoToMsg(int errNo);

/*
 * gets the file at the specified txid and writes to given file descriptor
 * in params: bitdbNode must be specified; specify dirPath if directory; specify saveDirFp to save directory contents
 * queries at specified BitDB node
 * if foundHandler specified, will call to indicate if file is found before writing
 * returns appropriate status code
 */
CW_STATUS getFile(const char *txid, struct cwGetParams *params, int fd);

/*
 * reads from specified file stream to ascertain the desired txid from given directory/path
 * returns appropriate status code
 */
CW_STATUS dirPathToTxid(FILE *dirFp, const char *dirPath, char *pathTxid);

#endif
