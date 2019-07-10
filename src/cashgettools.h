#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include <curl/curl.h>
#include <b64/b64.h>
#include <mylist/mylist.h>
#include "cashwebuni.h"

#define CWG_DIR_NO 1
#define CWG_FETCH_NO 2
#define CWG_METADATA_NO 3
#define CWG_SYS_ERR 4
#define CWG_FETCH_ERR 5
#define CWG_WRITE_ERR 6
#define CWG_FILE_ERR 7
#define CWG_FILE_LEN_ERR 8
#define CWG_FILE_DEPTH_ERR 9

struct cwgGetParams {
	char *bitdbNode;
	char *dirPath;
	FILE *saveDirFp;
	void *extraData;
};

static inline void initCwgGetParams(struct cwgGetParams *cgp, char *bitdbNode) {
	cgp->bitdbNode = bitdbNode;
	cgp->dirPath = NULL;
	cgp->saveDirFp = NULL;
	cgp->extraData = NULL;
}

/*
 * returns generic error message by error code
 */
char *errNoToMsg(int errNo);

/*
 * gets the file at the specified txid and writes to given file descriptor
 * in params: bitdbNode must be specified; specify dirPath if directory; specify saveDirFp to save directory contents
 * queries at specified BitDB node
 * if foundHandler specified, will call to indicate if file is found before writing
 * returns appropriate status code
 */
CW_STATUS getFile(const char *txid, struct cwgGetParams *params, void (*foundHandler) (CW_STATUS, void *, int), int fd);

/*
 * reads from specified file stream to ascertain the desired txid from given directory/path
 * returns appropriate status code
 */
CW_STATUS dirPathToTxid(FILE *dirFp, const char *dirPath, char *pathTxid);

#endif
