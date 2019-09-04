#ifndef __CASHGETTOOLS_H__
#define __CASHGETTOOLS_H__

#include "cashwebuni.h"

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

/* can be set to redirect cashgettools error logging; defaults to stderr */
extern FILE *CWG_err_stream;

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
 * params for getting
 * mongodb: MongoDB address (assumed to be populated by BitDB Node); indicates for cashgettools to handle initialization/cleanup of mongoc environment/client
 * mongodbCli: Optionally initialize/set mongoc client yourself; if so, is the user's responsibility to cleanup mongoc client and environment.
 	       Do NOT set mongodb address above if this is the case; only one of these should be set, or behavior is undefined;
	       must be cast from type mongoc_client_t * (as such, MongoC library must be included/linked in user project if user-managed).
  	       Is primarily included for internal management by cashgettools
 * mongodbCliPool: Optionally initialize/set mongoc client pool yourself, for use in multi-threaded scenarios; if so, must handle cleanup of mongoc pool/environment;
		   must be cast from type mongoc_client_pool_t * (as such, MongoC library must be included/linked in user project if user-managed);
 * 		   may utilize CWG_init_mongo_pool and CWG_cleanup_mongo_pool when not user-managed (recommended)
 * bitdbNode: BitDB Node HTTP endpoint address; only specify if not using the former
 * bitdbRequestLimit: Specify whether or not BitDB Node has request limit 
 * dirPath: Forces requested file to be treated as directory index (checked for validity) and gets at path dirPath;
 	    May be useful if getting by means other than cashweb path ID
 * forceDir: Forces requested file to be treated as directory index;
 	     this means if true, an invalid directory index will return error CWG_IS_DIR_NO, even if file is valid.
	     Even if dirPath is specified, will be ignored in favor of writing directory index data at requested identifier
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
	void *mongodbCli;
	void *mongodbCliPool;
	const char *bitdbNode;
	bool bitdbRequestLimit;
	char *dirPath;
	bool forceDir;
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
void init_CWG_params(struct CWG_params *cgp, const char *mongodb, const char *bitdbNode, char (*saveMimeStr)[CWG_MIMESTR_BUF]);

/*
 * copies struct CWG_params from source to dest
 */
void copy_CWG_params(struct CWG_params *dest, struct CWG_params *source);

/*
 * gets the file by protocol-compliant identifier and writes to given file descriptor
 * this could be of the following formats:
   1) hex string of length TXID_CHARS (valid txid)
   2) name prefixed with CW_NAMETAG_PREFIX (e.g. $coolcashwebname, gets latest revision)
   3) name prefixed with revision number and CW_NAMETAG_PREFIX (e.g. 0$coolcashwebname, gets first revision)
   4) any above form of ID preceding a path (e.g. 0$coolcashwebname/coolpath/coolfile.cash)
 * if foundHandler specified, will call to indicate if file is found before writing
 * it recommended that fd be set blocking (~O_NONBLOCK)
 */
CW_STATUS CWG_get_by_id(const char *id, struct CWG_params *params, int fd);

/*
 * gets the file at the specified txid and writes to given file descriptor
 * queries at specified BitDB-populated MongoDB or BitDB HTTP endpoint
 * if foundHandler specified, will call to indicate if file is found before writing
 * it recommended that fd be set blocking (~O_NONBLOCK)
 */
CW_STATUS CWG_get_by_txid(const char *txid, struct CWG_params *params, int fd);

/*
 * gets the file at the specified nametag and writes to given file descriptor
 * specify revision for versioning; CW_REV_LATEST for latest revision
 * queries at specified BitDB-populated MongoDB or BitDB HTTP endpoint
 * if foundHandler specified, will call to indicate if file is found before writing
 * it recommended that fd be set blocking (~O_NONBLOCK)
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
 * writes identifier to specified location in memory,
   and remaining path to subPath if only partial path used (presumed to be referencing another directory), or NULL if full path used
 * pass NULL for subPath if only accepting exact match (won't match a partial path)
 * both subPath and pathId must be freed afterward
 */
CW_STATUS CWG_dirindex_path_to_identifier(FILE *indexFp, const char *path, char **subPath, char **pathId);

/*
 * reads directory index data and dumps as readable JSON format to given stream
 */
CW_STATUS CWG_dirindex_raw_to_json(FILE *indexFp, FILE *indexJsonFp);

/*
 * initializes MongoDB environment and client pool for multi-threaded scenarios
 * will set params->mongodbCliPool on success
 * must call CWG_cleanup_mongo_pool later on
 */
CW_STATUS CWG_init_mongo_pool(const char *mongodbAddr, struct CWG_params *params);

/*
 * cleans up MongoDB client pool (stored in params) and environment;
 * params->mongodbCliPool will be set NULL
 */
void CWG_cleanup_mongo_pool(struct CWG_params *params);

/*
 * returns generic error message by error code
 */
const char *CWG_errno_to_msg(int errNo);

#endif
