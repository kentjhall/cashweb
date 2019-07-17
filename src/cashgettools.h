#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include <curl/curl.h>
#include <mongoc.h>
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

/*
 * mongodb: MongoDB address (assumed to be populated by BitDB Node); only specify if not using the latter
 * mongodbCli: Optionally initialize/set mongoc client yourself (do NOT set mongodb if this is the case), but this probably isn't necessary;
 * 	       is included for internal management by cashgettools
 * bitdbNode: BitDB Node HTTP endpoint address; only specify if not using the former
 * bitdbRequestLimit: Specify whether or not BitDB Node has request limit
 * dirPath: Specify path if requesting a directory
 * saveDirFp: Optionally specify stream for writing directory contents if requesting a directory
 * foundHandler: Function to call when file is found, before writing
 * foundHandleData: Data pointer to pass to foundHandler()
 * foundSuppressErr: Specify an error code to suppress if file is found; <0 for none
 */
struct cwGetParams {
	const char *mongodb;
	mongoc_client_t *mongodbCli;
	const char *bitdbNode;
	bool bitdbRequestLimit;
	char *dirPath;
	FILE *saveDirFp;
	void (*foundHandler) (CW_STATUS, void *, int);
	void *foundHandleData;
	CW_STATUS foundSuppressErr;
};

/*
 * either MongoDB or BitDB HTTP endpoint address must be specified on init
 * if both specified, will default to MongoDB within cashgettools
 */ 
static inline void initCwGetParams(struct cwGetParams *cgp, const char *mongodb, const char *bitdbNode) {
	if (!mongodb && !bitdbNode) {
		fprintf(stderr, "ERROR: cashgettools params must be provided with address for MongoDB or BitDB HTTP endpoint on init; exit");
		die(NULL);
	} 
	cgp->mongodb = mongodb;
	cgp->mongodbCli = NULL;
	cgp->bitdbNode = bitdbNode;
	cgp->bitdbRequestLimit = true;
	cgp->dirPath = NULL;
	cgp->saveDirFp = NULL;
	cgp->foundHandler = NULL;
	cgp->foundHandleData = NULL;
	cgp->foundSuppressErr = -1;
}

static inline void copyCwGetParams(struct cwGetParams *dest, struct cwGetParams *source) {
	dest->mongodb = source->mongodb;
	dest->mongodbCli = source->mongodbCli;
	dest->bitdbNode = source->bitdbNode;
	dest->dirPath = source->dirPath;
	dest->saveDirFp = source->saveDirFp;
	dest->foundHandler = source->foundHandler;
	dest->foundHandleData = source->foundHandleData;
	dest->foundSuppressErr = source->foundSuppressErr;
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
