#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include "cashwebuni.h"
#include <curl/curl.h>
#include <mongoc.h>
#include <b64/b64.h>
#include <mylist/mylist.h>

/* cashgettools error codes */
#define CWG_IN_DIR_NO CW_SYS_ERR+1
#define CWG_IS_DIR_NO CW_SYS_ERR+2
#define CWG_FETCH_NO CW_SYS_ERR+3
#define CWG_METADATA_NO CW_SYS_ERR+4
#define CWG_SCRIPT_CODE_NO CW_SYS_ERR+5
#define CWG_SCRIPT_NO CW_SYS_ERR+6
#define CWG_FETCH_ERR CW_SYS_ERR+7
#define CWG_WRITE_ERR CW_SYS_ERR+8
#define CWG_SCRIPT_ERR CW_SYS_ERR+9
#define CWG_FILE_ERR CW_SYS_ERR+10
#define CWG_FILE_LEN_ERR CW_SYS_ERR+11
#define CWG_FILE_DEPTH_ERR CW_SYS_ERR+12

/* this is passed to CWG_get_by_nametag if seeking latest revision; for readability */
#define CWG_REV_LATEST -1

/* required array size if passing saveMimeStr in params */
#define CWG_MIMESTR_BUF 256

/*
 * params for getting
 * mongodb: MongoDB address (assumed to be populated by BitDB Node); only specify if not using the latter
 * mongodbCli: Optionally initialize/set mongoc client yourself (do NOT set mongodb if this is the case), but this probably isn't necessary;
 * 	       is included for internal management by cashgettools
 * bitdbNode: BitDB Node HTTP endpoint address; only specify if not using the former
 * bitdbRequestLimit: Specify whether or not BitDB Node has request limit
 * dirPath: Specify path if requesting a directory
 * dirPathReplace: Optionally allow requested dirPath to be replaced by nametag scripting; set NULL if not desired.
 		   If set, string length of zero indicates to allow replacement by script, but not mandated;
		   otherwise, will replace with set string if not overwritten by script (soft replacement).
		   This pointer CAN be overwritten, so if string is heap-allocated, a separate reference should be maintained for freeing it
 * dirPathReplaceToFree: Specifies whether dirPathReplace points to heap-allocated memory that needs to be freed on replacement; primarily for internal use.
 			 Should function if set true when initial dirPathReplace points to heap-allocated memory,
			 but is strongly recommended that freeing any passed pointer be handled by the user instead
 * saveDirFp: Optionally specify stream for writing directory contents if requesting a directory
 * saveMimeStr: Optionally interpret/save file's mimetype string to this memory location;
 		pass pointer to char array of length CWG_MIMESTR_BUF (this #define is available in header)
 * foundHandler: Function to call when file is found, before writing
 * foundHandleData: Data pointer to pass to foundHandler()
 * foundSuppressErr: Specify an error code to suppress if file is found; <0 for none
 * datadir: specify data directory path for cashwebtools;
 	    can be left as NULL if cashwebtools is properly installed on system with 'make install'
 */
struct CWG_params {
	const char *mongodb;
	mongoc_client_t *mongodbCli;
	const char *bitdbNode;
	bool bitdbRequestLimit;
	char *dirPath;
	char *dirPathReplace;
	bool dirPathReplaceToFree;
	FILE *saveDirFp;
	char (*saveMimeStr)[CWG_MIMESTR_BUF];
	void (*foundHandler) (CW_STATUS, void *, int);
	void *foundHandleData;
	CW_STATUS foundSuppressErr;
	const char *datadir;
};

/*
 * initialize struct CWG_params
 * either MongoDB or BitDB HTTP endpoint address must be specified on init
 * if both specified, will default to MongoDB within cashgettools
 * if saving mime type string is desired, pointer must be passed here for array initialization; otherwise, can be set NULL
 */ 
static inline void init_CWG_params(struct CWG_params *cgp, const char *mongodb, const char *bitdbNode, char (*saveMimeStr)[CWG_MIMESTR_BUF]) {
	if (!mongodb && !bitdbNode) {
		fprintf(stderr, "WARNING: CWG_params must be provided with address for MongoDB or BitDB HTTP endpoint on init\n");
	} 
	cgp->mongodb = mongodb;
	cgp->mongodbCli = NULL;
	cgp->bitdbNode = bitdbNode;
	cgp->bitdbRequestLimit = true;
	cgp->dirPath = NULL;
	cgp->dirPathReplace = NULL;
	cgp->dirPathReplaceToFree = false;
	cgp->saveDirFp = NULL;
	cgp->saveMimeStr = saveMimeStr;
	if (cgp->saveMimeStr) { memset(*cgp->saveMimeStr, 0, sizeof(*cgp->saveMimeStr)); }
	cgp->foundHandler = NULL;
	cgp->foundHandleData = NULL;
	cgp->foundSuppressErr = -1;
	cgp->datadir = NULL;
}

/*
 * copy struct CWG_params from source to dest
 */
static inline void copy_CWG_params(struct CWG_params *dest, struct CWG_params *source) {
	dest->mongodb = source->mongodb;
	dest->mongodbCli = source->mongodbCli;
	dest->bitdbNode = source->bitdbNode;
	dest->dirPath = source->dirPath;
	dest->dirPathReplace = source->dirPathReplace;
	dest->dirPathReplaceToFree = source->dirPathReplaceToFree;
	dest->saveDirFp = source->saveDirFp;
	dest->saveMimeStr = source->saveMimeStr;
	dest->foundHandler = source->foundHandler;
	dest->foundHandleData = source->foundHandleData;
	dest->foundSuppressErr = source->foundSuppressErr;
	dest->datadir = source->datadir;
}

/*
 * gets the file at the specified txid and writes to given file descriptor
 * queries at specified BitDB-populated MongoDB or BitDB HTTP endpoint
 * if foundHandler specified, will call to indicate if file is found before writing
 */
CW_STATUS CWG_get_by_txid(const char *txid, struct CWG_params *params, int fd);

/*
 * gets the file at the specified nametag and writes to given file descriptor
 * specify revision for versioning; CWG_REV_LATEST for latest revision
 * queries at specified BitDB-populated MongoDB or BitDB HTTP endpoint
 * if foundHandler specified, will call to indicate if file is found before writing
 */
CW_STATUS CWG_get_by_nametag(const char *name, int revision, struct CWG_params *params, int fd);

/*
 * reads from specified file stream to ascertain the desired txid from given directory/path;
 * writes txid to specified location in memory
 */
CW_STATUS CWG_dir_path_to_identifier(FILE *dirFp, const char *dirPath, char *pathTxid);

/*
 * returns generic error message by error code
 */
const char *CWG_errno_to_msg(int errNo);

#endif
