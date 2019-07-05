#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include <curl/curl.h>
#include <b64/b64.h>
#include <mylist/mylist.h>
#include "cashwebuni.h"

#define CWG_OK 0
#define CWG_DIR_NO 1
#define CWG_FETCH_NO 2
#define CWG_FETCH_ERR 3
#define CWG_WRITE_ERR 4
#define CWG_FILE_ERR 5

/*
 * returns generic error message by error code
 */
char *errNoToMsg(int errNo);

/*
 * gets the file at the specified txid and writes to given file descriptor
 * queries at specified BitDB node
 * if foundHandler specified, will call to indicate if file is found before writing
 * returns appropriate status code
 */
int getFile(const char *txid, const char *bdNode, void (*foundHandler) (int, int), int fd);

/*
 * reads from specified file stream to ascertain the desired txid from given directory/path
 * returns appropriate status code
 */
int dirPathToTxid(FILE *dirFp, const char *dirPath, char *pathTxid);

/*
 * gets the file at the given path of the directory at specified txid and writes to given file descriptor
 * queries at specified BitDB node
 * if writeDirFp is specified, the directory is written there
 * if foundHandler specified, will call to indicate if file is found before writing
 * returns appropriate status code
 */
int getDirFile(const char *dirTxid, const char *dirPath, const char *bdNode, FILE *writeDirFp, void (*foundHandler) (int, int), int fd);

#endif
