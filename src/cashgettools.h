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
#define CWG_SCRIPT_REV_NO CW_SYS_ERR+5
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
 * dirPath: Forces file at requested ID to be treated as directory index, and gets at requested path;
 	    allows getting at path even when getting by CWG_get_by_txid or CWG_get_by_name
 * dirPathReplace: Optionally allow a requested path to be replaced by nametag scripting; set NULL if not desired.
 		   If set, string length of zero indicates to allow replacement by script, but not mandated;
		   otherwise, will replace with set string if not displaced by script (soft replacement).
 * dirPathReplaceToFree: Specifies whether dirPathReplace points to heap-allocated memory that needs to be freed on replacement; primarily for internal use.
 			 Should function if set true when initial dirPathReplace points to heap-allocated memory,
			 but is strongly recommended that freeing any passed pointer be handled by the user instead
 * saveMimeStr: Optionally interpret/save file's mimetype string to this memory location;
 		pass pointer to char array of length CWG_MIMESTR_BUF (this #define is available in header).
		Will result as string of length 0 if file is of type CW_T_FILE, CW_T_DIR, or otherwise invalid value
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
 * initializes struct CWG_params
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
	cgp->datadir = CW_INSTALL_DATADIR_PATH;
}

/*
 * copies struct CWG_params from source to dest
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
 * struct for carrying info on file at specific txid; stores metadata and interpreted mimetype string
 */
struct CWG_file_info {
	struct CW_file_metadata metadata;
	char mimetype[CWG_MIMESTR_BUF];
};

/*
 * initializes struct CWG_file_info
 */
static inline void init_CWG_file_info(struct CWG_file_info *cfi) {
	cfi->mimetype[0] = 0;
}

/*
 * struct for carrying info on a nametag when analyzing; pointers must be exclusively heap-allocated
 * always make sure to initialize on use and destroy afterward
 * revisionTxid: the txid to be used for further revisions on the nametag; will be NULL if immutable
 * revision: the revision that the nametag is up to
 * nameRefs: a NULL-terminated array of names directly referenced by the nametag script; if present, nametag is presumed to be mutable by reference
 * txidRefs: a NULL-terminated array of txids directly referenced by the nametag script
 */
struct CWG_nametag_info {
	char *revisionTxid;
	int revision;
	char **nameRefs;
	char **txidRefs;
};

/*
 * initializes struct CWG_nametag_info
 */
static inline void init_CWG_nametag_info(struct CWG_nametag_info *cni) {
	cni->revisionTxid = NULL;
	cni->revision = 0;
	cni->nameRefs = NULL;
	cni->txidRefs = NULL;
}

/*
 * frees heap-allocated data pointed to by given struct CWG_nametag_info
 */
static inline void destroy_CWG_nametag_info(struct CWG_nametag_info *cni) {
	if (cni->revisionTxid) { free(cni->revisionTxid); }
	if (cni->nameRefs) {
		char **nameRefsPtr = cni->nameRefs;
		while (*nameRefsPtr) { free(*nameRefsPtr++); }
		free(cni->nameRefs);
	}
	if (cni->txidRefs) {
		char **txidRefsPtr = cni->txidRefs;
		while (*txidRefsPtr) { free(*txidRefsPtr++); }
		free(cni->txidRefs);
	}
	init_CWG_nametag_info(cni);
}

/*
 * gets the file by protocol-compliant identifier and writes to given file descriptor
 * this could be of the following formats:
   1) hex string of length TXID_CHARS (valid txid)
   2) name prefixed with CW_NAMETAG_PREFIX (e.g. $coolcashwebname, gets latest revision)
   3) name prefixed with revision number and CW_NAMETAG_PREFIX (e.g. 0$coolcashwebname, gets first revision)
   4) any above form of ID preceding a path (e.g. 0$coolcashwebname/coolpath/coolfile.cash)
 */
CW_STATUS CWG_get_by_id(const char *id, struct CWG_params *params, int fd);

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
 * gets file info by txid and writes to given struct CWG_file_info
 * if info->mimetype results in empty string, file has no specified mimetype (most likely CW_T_FILE or CW_T_DIR); may be treated as binary data
 */
CW_STATUS CWG_get_file_info(const char *txid, struct CWG_params *params, struct CWG_file_info *info);

/*
 * gets nametag info by name/revision and writes to given struct CWG_nametag_info
 * always cleanup afterward with destroy_CWG_nametag_info() for freeing struct data
 */
CW_STATUS CWG_get_nametag_info(const char *name, int revision, struct CWG_params *params, struct CWG_nametag_info *info);

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
