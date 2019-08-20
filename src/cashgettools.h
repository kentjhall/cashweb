#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include "cashwebuni.h"
#include <curl/curl.h>
#include <mongoc.h>

/* cashgettools status codes */
#define CWG_IN_DIR_NO CW_SYS_ERR+1
#define CWG_IS_DIR_NO CW_SYS_ERR+2
#define CWG_FETCH_NO CW_SYS_ERR+3
#define CWG_METADATA_NO CW_SYS_ERR+4
#define CWG_SCRIPT_CODE_NO CW_SYS_ERR+5
#define CWG_SCRIPT_NO CW_SYS_ERR+6
#define CWG_CIRCLEREF_NO CW_SYS_ERR+7
#define CWG_FETCH_ERR CW_SYS_ERR+8
#define CWG_WRITE_ERR CW_SYS_ERR+9
#define CWG_SCRIPT_ERR CW_SYS_ERR+10
#define CWG_SCRIPT_RETRY_ERR CW_SYS_ERR+11
#define CWG_FILE_ERR CW_SYS_ERR+12
#define CWG_FILE_LEN_ERR CW_SYS_ERR+13
#define CWG_FILE_DEPTH_ERR CW_SYS_ERR+14

/* required array size if passing saveMimeStr in params */
#define CWG_MIMESTR_BUF 256

/*
 * params for getting
 * mongodb: MongoDB address (assumed to be populated by BitDB Node); only specify if not using the latter
 * mongodbCli: Optionally initialize/set mongoc client yourself (do NOT set mongodb if this is the case), but this probably isn't necessary;
 * 	       is included for internal management by cashgettools
 * bitdbNode: BitDB Node HTTP endpoint address; only specify if not using the former
 * bitdbRequestLimit: Specify whether or not BitDB Node has request limit
 * dirPath: Specify path if requesting a directory;
 	    keep in mind this pointer may be overwritten when getting by ID (specifically a path ID) via CWG_get_by_id
 * dirPathReplace: Optionally allow requested dirPath to be replaced by nametag scripting; set NULL if not desired.
 		   If set, string length of zero indicates to allow replacement by script, but not mandated;
		   otherwise, will replace with set string if not overwritten by script (soft replacement).
 * dirPathReplaceToFree: Specifies whether dirPathReplace points to heap-allocated memory that needs to be freed on replacement; primarily for internal use.
 			 Should function if set true when initial dirPathReplace points to heap-allocated memory,
			 but is strongly recommended that freeing any passed pointer be handled by the user instead
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
	cgp->dirPathReplace = "";
	cgp->dirPathReplaceToFree = false;
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
 * specify revision for versioning; CW_REV_LATEST for latest revision
 * queries at specified BitDB-populated MongoDB or BitDB HTTP endpoint
 * if foundHandler specified, will call to indicate if file is found before writing
 */
CW_STATUS CWG_get_by_name(const char *name, int revision, struct CWG_params *params, int fd);

/*
 * gets the file by protocol-compliant identifier and writes to given file descriptor
 * this could be of the following formats:
   1) hex string of length TXID_CHARS (valid txid)
   2) name prefixed with CW_NAMETAG_PREFIX (e.g. ~coolcashwebname, gets latest revision)
   3) name prefixed with revision number and CW_NAMETAG_PREFIX (e.g. 0~coolcashwebname, gets first revision)
 */
CW_STATUS CWG_get_by_id(const char *id, struct CWG_params *params, int fd);

/*
 * reads from specified file stream to ascertain the desired file identifier from given directory/path;
   if path is prepended with '/', this will be ignored (i.e. handled, but not necessary)
 * writes identifier to specified location in memory; ensure CW_NAMETAG_ID_MAX_LEN+1 allocated if this could be nametag reference, or CW_TXID_CHARS+1 otherwise;
   writes remaining path to subPath if only partial path used (presumed to be referencing another directory), or NULL if full path used
 * pass NULL for subPath if only accepting exact match (won't match a partial path)
 */
CW_STATUS CWG_dirindex_path_to_identifier(FILE *indexFp, const char *path, char const **subPath, char *pathId);

/*
 * reads directory index data and dumps as readable JSON format to given stream
 */
CW_STATUS CWG_dirindex_raw_to_json(FILE *indexFp, FILE *indexJsonFp);

/*
 * returns generic error message by error code
 */
const char *CWG_errno_to_msg(int errNo);

#endif
