#include <pthread.h>
#include <fcntl.h>
#include <mylist/mylist.h>
#include "cashgettools.h"
#include "cashwebutils.h"

/* stream for logging errors; defaults to stderr */
FILE *CWG_err_stream = NULL;

#include "cashfetchutils.h"

/* general constants */
#define LINE_BUF 150

/*
 * struct for information to carry around during script execution
 * if counter is set, nothing will actually be fetched
 */
struct CWG_script_pack {
	List *scriptStreams;
	List *fetchedNames;
	const char *revTxid;
	const char *revTxidFirst;
	int atRev;
	int maxRev;
	struct CWG_nametag_counter *infoCounter;
};

/*
 * initializes struct CWG_script_pack
 */
static inline void init_CWG_script_pack(struct CWG_script_pack *sp, List *scriptStreams, List *fetchedNames, const char *revTxid, int maxRev);

/*
 * copies struct CWG_script_pack from source to dest and increments current revision (atRev)
 */
static inline void copy_inc_CWG_script_pack(struct CWG_script_pack *dest, struct CWG_script_pack *source);

/*
 * struct for tracking info on a nametag when just analyzing; pointers all heap-allocated
 * corresponds to struct CWG_nametag_info, but for internal use
 */
struct CWG_nametag_counter {
	char *revisionTxid;
	int revision;
	List nameRefs;
	List txidRefs;
};

/*
 * initializes given struct CWG_nametag_counter
 */
static inline void init_CWG_nametag_counter(struct CWG_nametag_counter *cnc);

/*
 * frees heap-allocated memory of given struct CWG_nametag_counter for cleanup
 */
static inline void destroy_CWG_nametag_counter(struct CWG_nametag_counter *cnc);

/*
 * copies data from specified struct CWG_nametag_counter into given struct CWG_nametag_info
 * this exists to turn locally available Lists from CWG_nametag_counter into more generalizable NULL-terminated arrays in CWG_nametag_info
 */
static bool counter_copy_CWG_nametag_info(struct CWG_nametag_info *cni, struct CWG_nametag_counter *cnc);

/* 
 * currently, simply reports a warning if given protocol version is newer than one in use
 * may be made more robust in the future
 */
static void protocolCheck(uint16_t pVer);

/*
 * determines mime type from given cashweb type and copies to location in struct CWG_params if not NULL
 * if unable to resolve, or if type CW_T_FILE, CW_T_DIR, or CW_T_MIMESET is provided, defaults to MIME_STR_DEFAULT
 */
static CW_STATUS cwTypeToMimeStr(CW_TYPE type, struct CWG_params *cgp);

/*
 * translates given hex string to byte data and writes to file descriptor
 */
static CW_STATUS writeHexDataStr(const char *hexDataStr, int suffixLen, int fd);

/*
 * resolves file metadata from given hex data string according to protocol format,
 * and save to given struct pointer
 */
static CW_STATUS hexResolveMetadata(const char *hexData, struct CW_file_metadata *md);

/*
 * recursively traverse file tree from root hexdata
 * partialTxids Lists are for keeping track of partials in chained tree (between linked root hexdatas)
 * stops at depth specified in md
 */
static CW_STATUS traverseFileTree(const char *treeHexData, List *partialTxids[], int suffixLen, int depth,
			    	  struct CWG_params *params, struct CW_file_metadata *md, int fd);

/*
 * traverse file chain from starting hexdata
 * stops at length specified in md
 */
static CW_STATUS traverseFileChain(const char *hexDataStart, struct CWG_params *params, struct CW_file_metadata *md, int fd);

/*
 * wrapper for determining whether to traverse file as chain or tree
 */
static inline CW_STATUS traverseFile(const char *hexDataStart, struct CWG_params *params, struct CW_file_metadata *md, int fd);

/*
 * frees all heap allocations and closes file descriptors for List of file descriptors
 */
static inline void freeFdStack(List *fdStack);

/*
 * writes path link (for directory index) to descriptor fd;
   intended exclusively for use in scripting, expected prior to writing directory index
 */
static inline CW_STATUS writePathLink(const char *pathR, const char *linkR, int fd);

/*
 * execute necessary action for given CW_OPCODE c
 * may involve pushing/popping stack (including fdStack), reading from specified scriptStream, and/or writing to fd
 * fdStack is for storing open file descriptors used for storage during script execution
 */
static CW_STATUS execScriptCode(CW_OPCODE c, FILE *scriptStream, List *stack, List *fdStack, struct CWG_script_pack *sp, struct CWG_params *params, int fd);

/*
 * executes cashweb script from scriptStream, writing anything specified by script to file descriptor fd
 * revTxid may be set NULL in given struct CWS_script_pack if reading from existing script streams for revisioning
 */
static CW_STATUS execScript(struct CWG_script_pack *sp, struct CWG_params *params, int fd);

/*
 * starting point for executing the beginning of a cashweb script (not on a per-revision basis);
 * contains some stuff that needs to avoid the recursiveness of execScript()
 */
static CW_STATUS execScriptStart(struct CWG_script_pack *sp, struct CWG_params *params, int fd);

/*
 * fetches/traverses script data at nametag and writes to stream
 * writes txid of of script to txid
 */
static CW_STATUS getScriptByNametag(const char *name, struct CWG_params *params, char **txidPtr, FILE *stream);

/*
 * fetches/traverses script data at tx with given input txid (and vout CW_REVISION_INPUT_VOUT) and writes to stream
 * writes txid of of script to txid
 */
static CW_STATUS getScriptByInTxid(const char *inTxid, struct CWG_params *params, char **txidPtr, FILE *stream);

/*
 * fetched/traverses file at specified path of given directory index stream dirFp, writing file to specified file descriptor
 * fetchedNames will track origin nametag(s) for chained script/directory nametag references; should be set NULL on initial call
 * responsible for calling foundHandler if present in params; will be set to NULL upon call
 */
static CW_STATUS getFileByPath(FILE *dirFp, const char *path, List *fetchedNames, struct CWG_params *params, int fd);

/*
 * convenience wrapper function for getByGetterPath when getting for path at nametag revision
 */
static inline CW_STATUS getFileByNametagPath(const char *name, int revision, const char *path, List *fetchedNames, struct CWG_params *params, int fd);

/*
 * fetches/traverses file at given nametag (according to script at nametag) and writes to specified file descriptor;
   or, if counter is set (non-NULL), nametag info will be saved to it and nothing will be written to file descriptor
 * revision can be specified for which version to get; CW_REV_LATEST for latest
 * fetchedNames will track origin nametag(s) for chained script/directory nametag references; should be set NULL on initial call
 * responsible for calling foundHandler if present in params; will be set to NULL upon call
 */
static CW_STATUS getFileByNametag(const char *name, int revision, List *fetchedNames, struct CWG_params *params, struct CWG_nametag_counter *counter, int fd);

/*
 * convenience wrapper function for getByGetterPath when getting for path at txid
 */
static inline CW_STATUS getFileByTxidPath(const char *txid, const char *path, List *fetchedNames, struct CWG_params *params, int fd);

/*
 * fetches/traverses file at given txid and writes to specified file descriptor;
   or, if counter is set (non-NULL), file metadata will be saved to it and nothing will be written to file descriptor.
   NOTE: does not write mimetype string to counter; this is still saved to params, whose pointer can be set outside of getFileByTxid 
 * fetchedNames will track origin nametag(s) for chained script/directory nametag references; should be set NULL on initial call
 * responsible for calling foundHandler if present in params; will be set to NULL upon call
 */
static CW_STATUS getFileByTxid(const char *txid, List *fetchedNames, struct CWG_params *params, struct CWG_file_info *counter, int fd);

/*
 * convenience wrapper function for getByGetterPath when getting for path at cashweb id
 */
static inline CW_STATUS getFileByIdPath(const char *id, const char *path, List *fetchedNames, struct CWG_params *params, int fd);

/*
 * wrapper function for either getting by txid or by nametag, dependent on prefix (or lack thereof) of provided ID
 * fetchedNames will track origin nametag(s) for chained script/directory nametag references; should be set NULL on initial call
 * responsible for calling foundHandler if present in params; will be set to NULL upon call
 */
static CW_STATUS getFileById(const char *id, List *fetchedNames, struct CWG_params *params, int fd);

/*
 * struct CWG_getter stores a send function pointer and its arguments; strictly for internal use by cashsendtools
 * really only exists to avoid some repetitive code
 */
struct CWG_getter {
	CW_STATUS (*byId) (const char *, List *, struct CWG_params *, int);
	CW_STATUS (*byTxid) (const char *, List *, struct CWG_params *, struct CWG_file_info *, int);
	CW_STATUS (*byName) (const char *, int, List *, struct CWG_params *, struct CWG_nametag_counter *, int);
	const char *id;
	const char *name;
	int revision;
	List *fetchedNames;
	struct CWG_params *params;
};

/*
 * initializes struct CWG_getter for getting by general cashweb id
 */
static inline void init_CWG_getter_for_id(struct CWG_getter *cgg, const char *id, List *fetchedNames, struct CWG_params *params);

/*
 * initializes struct CWG_getter for getting specifically by txid
 */
static inline void init_CWG_getter_for_txid(struct CWG_getter *cgg, const char *txid, List *fetchedNames, struct CWG_params *params);

/*
 * initializes struct CWG_getter for getting by name/revision
 */
static inline void init_CWG_getter_for_name(struct CWG_getter *cgg, const char *name, int revision, List *fetchedNames, struct CWG_params *params);

/*
 * convenience function for getting as per contents of given struct CWG_getter, regardless of whether by name or id
 */
static inline CW_STATUS getByGetter(struct CWG_getter *getter, int fd);

/*
 * fetches/traverses directory by given struct CWG_getter, and then file at given path, writing file to specified file descriptor
 * if path is NULL, this function is equivalent to getByGetter
 */
static CW_STATUS getByGetterPath(struct CWG_getter *getter, const char *path, int fd);

/* ------------------------------------- PUBLIC ------------------------------------- */

void init_CWG_params(struct CWG_params *cgp, const char *mongodb, const char *bitdbNode, const char *restEndpoint, char (*saveMimeStr)[CWG_MIMESTR_BUF]) {
	cgp->mongodb = mongodb;
	cgp->mongodbCli = NULL;
	cgp->mongodbCliPool = NULL;
	cgp->bitdbNode = bitdbNode;
	cgp->restEndpoint = restEndpoint;
	cgp->requestLimit = true;
	cgp->dirPath = NULL;
	cgp->forceDir = false;
	cgp->saveMimeStr = saveMimeStr;
	if (cgp->saveMimeStr) { memset(*cgp->saveMimeStr, 0, sizeof(*cgp->saveMimeStr)); }
	cgp->foundHandler = NULL;
	cgp->foundHandleData = NULL;
	cgp->foundSuppressErr = -1;
	cgp->datadir = CW_INSTALL_DATADIR_PATH;
}

void copy_CWG_params(struct CWG_params *dest, struct CWG_params *source) {
	dest->mongodb = source->mongodb;
	dest->mongodbCli = source->mongodbCli;
	dest->mongodbCliPool = source->mongodbCliPool;
	dest->bitdbNode = source->bitdbNode;
	dest->restEndpoint = source->restEndpoint;
	dest->requestLimit = source->requestLimit;
	dest->dirPath = source->dirPath;
	dest->forceDir = source->forceDir;
	dest->saveMimeStr = source->saveMimeStr;
	dest->foundHandler = source->foundHandler;
	dest->foundHandleData = source->foundHandleData;
	dest->foundSuppressErr = source->foundSuppressErr;
	dest->datadir = source->datadir;
}

CW_STATUS CWG_get_by_id(const char *id, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; } 

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	if ((status = getFileByIdPath(id, params->dirPath, NULL, params, fd)) == CW_CALL_NO) {
		fprintf(CWG_err_stream, "CWG_get_by_id provided with invalid identifier: %s\n", id);
		status = CWG_CALL_ID_NO;
	}
	params->foundHandler = savePtr;
	
	cleanupFetcher(params);
	return status;
}

CW_STATUS CWG_get_by_txid(const char *txid, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; } 	

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	status = getFileByTxidPath(txid, params->dirPath, NULL, params, fd);
	params->foundHandler = savePtr;
	
	cleanupFetcher(params);
	return status;
}

CW_STATUS CWG_get_by_name(const char *name, int revision, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; } 

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	status = getFileByNametagPath(name, revision, params->dirPath, NULL, params, fd);
	params->foundHandler = savePtr;
	
	cleanupFetcher(params);
	return status;
}

CW_STATUS CWG_get_file_info(const char *txid, struct CWG_params *params, struct CWG_file_info *info) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; }

	int devnull = open("/dev/null", O_WRONLY);
	if (devnull < 0) { perror("open() /dev/null failed"); cleanupFetcher(params); return CW_SYS_ERR; }

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	char (*saveStrPtr)[CWG_MIMESTR_BUF] = params->saveMimeStr;
	params->saveMimeStr = &info->mimetype;
	status = getFileByTxid(txid, NULL, params, info, devnull);
	params->saveMimeStr = saveStrPtr;
	params->foundHandler = savePtr;

	close(devnull);
	cleanupFetcher(params);
	return status;
}

CW_STATUS CWG_get_nametag_info(const char *name, int revision, struct CWG_params *params, struct CWG_nametag_info *info) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; }

	int devnull = open("/dev/null", O_WRONLY);
	if (devnull < 0) { perror("open() /dev/null failed"); cleanupFetcher(params); return CW_SYS_ERR; }

	struct CWG_nametag_counter counter;
	init_CWG_nametag_counter(&counter);

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	if ((status = getFileByNametag(name, revision, NULL, params, &counter, devnull)) != CW_OK) { goto cleanup; }
	params->foundHandler = savePtr;

	if (!counter_copy_CWG_nametag_info(info, &counter)) { destroy_CWG_nametag_info(info); status = CW_SYS_ERR; goto cleanup; }
	if (info->revisionTxid && revision >= 0 && revision <= info->revision) { free(info->revisionTxid); info->revisionTxid = NULL; }	

	cleanup:
		destroy_CWG_nametag_counter(&counter);
		close(devnull);
		cleanupFetcher(params);
		return status;
}

CW_STATUS CWG_dirindex_path_to_identifier(FILE *indexFp, const char *path, char **subPath, char **pathId) {
	CW_STATUS status = CWG_IN_DIR_NO;
	*pathId = NULL;
	const char *dirPath = path[0] == '/' ? path+1 : path;

	char pathTxidBytes[CW_TXID_BYTES];
	memset(pathTxidBytes, 0, CW_TXID_BYTES);

	struct DynamicMemory line;
	initDynamicMemory(&line);
	
	char *linePath;
	size_t lineLen = 0;
	bool isSubDir = false;
	int count = 0;
	bool found = false;
	bool concluded = false;
	int readlineStatus;
	while ((readlineStatus = safeReadLine(&line, LINE_BUF, indexFp)) == READLINE_OK) {
		if (line.data[0] == 0) { concluded = true; break; }
				
		if (!found) {
			if (CW_is_valid_cashweb_id(line.data) || line.data[0] == '.') { --count; continue; }

			if (line.data[0] != '/') { break; }
			++count;
			linePath = line.data + 1;
			lineLen = strlen(linePath);
			isSubDir = linePath[lineLen-1] == '/';
			if (strcmp(dirPath, linePath) == 0 || (subPath && isSubDir && strncmp(dirPath, linePath, lineLen-1) == 0 && (dirPath[lineLen-1] == 0 || dirPath[lineLen-1] == '/'))) {
				found = true;
				status = CW_OK;
				if (subPath && isSubDir && strlen(dirPath) > lineLen-1) {
					if ((*subPath = strdup(dirPath + (lineLen-1))) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; goto cleanup; }
				} else { *subPath = NULL; }

				if ((readlineStatus = safeReadLine(&line, LINE_BUF, indexFp)) != READLINE_OK) { break; }
				if (line.data[0] == 0) { concluded = true; break; }

				if (CW_is_valid_cashweb_id(line.data)) {
					if ((*pathId = strdup(line.data)) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; goto cleanup; }
					count = 0;
					concluded = true;
					break;
				}
				else if (line.data[0] == '.') {
					status = CWG_dirindex_path_to_identifier(indexFp, line.data[1] == '/' ? line.data+1 : line.data, subPath, pathId);
					goto cleanup;
				}
			}
		}
	}
	if (ferror(indexFp)) { perror("fgets() failed on directory index"); status = CW_SYS_ERR; }
	else if (readlineStatus == READLINE_ERR) { status = CW_SYS_ERR; }
	else if (!concluded) { status = CWG_IS_DIR_NO; }
	if (status != CW_OK) { goto cleanup; }

	if (count > 0) {
		if (fseek(indexFp, CW_TXID_BYTES*(count-1), SEEK_CUR) < 0) {
			perror("fseek() SEEK_CUR failed on directory index");
			status = CWG_IS_DIR_NO;
			goto cleanup;
		}
		if (fread(pathTxidBytes, CW_TXID_BYTES, 1, indexFp) < 1) {
			if (ferror(indexFp)) {
				perror("fread() failed on directory index");
				status = CW_SYS_ERR;
			} else { status = CWG_IS_DIR_NO; }
			goto cleanup;
		}
		if ((*pathId = malloc(CW_TXID_CHARS+1)) == NULL) { perror("malloc failed"); status = CW_SYS_ERR; goto cleanup; }
		byteArrToHexStr(pathTxidBytes, CW_TXID_BYTES, *pathId);
	}
	if (*pathId == NULL) { status = CWG_IN_DIR_NO; } // this probably isn't necessary to set, as status starts at this value, but jic

	cleanup:
		freeDynamicMemory(&line);
		return status;
}

CW_STATUS CWG_dirindex_raw_to_json(FILE *indexFp, FILE *indexJsonFp) {
	json_t *indexJson;
	if ((indexJson = json_object()) == NULL) { perror("json_object() failed"); return CW_SYS_ERR; }

	CW_STATUS status = CW_OK;

	List paths;
	initList(&paths);

	struct DynamicMemory line;
	initDynamicMemory(&line);

	char *path;
	size_t count = 0;
	bool concluded = false;
	int readlineStatus;
	while ((readlineStatus = safeReadLine(&line, LINE_BUF, indexFp)) == READLINE_OK) {
		if (line.data[0] == 0) { concluded = true; break; }

		if (CW_is_valid_cashweb_id(line.data) || line.data[0] == '.') {
			if ((path = popFront(&paths)) == NULL) { status = CWG_IS_DIR_NO; goto cleanup; }		
			--count;
			json_object_set_new(indexJson, path+1, json_string(line.data));
			free(path);
			continue;
		}

		if (line.data[0] != '/') { break; }
		if ((path = strdup(line.data)) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; goto cleanup; }
		if (!addFront(&paths, path)) { perror("mylist addFront() failed"); status = CW_SYS_ERR; goto cleanup; }
		++count;
	}
	if (ferror(indexFp)) { perror("fgets() failed on directory index"); status = CW_SYS_ERR; }
	else if (readlineStatus == READLINE_ERR) { status = CW_SYS_ERR; }
	else if (!concluded) { status = CWG_IS_DIR_NO; }
	if (status != CW_OK) { goto cleanup; }	

	reverseList(&paths);

	char pathTxidBytes[CW_TXID_BYTES];
	memset(pathTxidBytes, 0, CW_TXID_BYTES);
	char txid[CW_TXID_CHARS+1];

	for (int i=0; i<count; i++) {
		if (fread(pathTxidBytes, CW_TXID_BYTES, 1, indexFp) < 1) {
			if (ferror(indexFp)) {
				perror("fread() failed on directory index");
				status = CW_SYS_ERR;
			} else { status = CWG_IS_DIR_NO; }	
			goto cleanup;
		}
		byteArrToHexStr(pathTxidBytes, CW_TXID_BYTES, txid);

		if ((path = popFront(&paths)) == NULL) {
			fprintf(CWG_err_stream, "unexpected empty list in CWG_dirindex_raw_to_json; problem with cashgettools\n");
			status = CW_SYS_ERR;
			goto cleanup;
		}
		json_object_set_new(indexJson, path+1, json_string(txid));
		free(path);
	}

	if (json_dumpf(indexJson, indexJsonFp, JSON_INDENT(4)) == -1) { perror("json_dumpf() failed"); status = CW_SYS_ERR; goto cleanup; }

	cleanup:
		freeDynamicMemory(&line);
		removeAllNodes(&paths, true);
		json_decref(indexJson);
		return status;
}

CW_STATUS CWG_init_mongo_pool(const char *mongodbAddr, struct CWG_params *params) {
	return initMongoPool(mongodbAddr, params);	
}

void CWG_cleanup_mongo_pool(struct CWG_params *params) {
	return cleanupMongoPool(params);	
}

const char *CWG_errno_to_msg(CW_STATUS errNo) {
	switch (errNo) {
		case CW_DATADIR_NO:
			return "Unable to find proper cashwebtools data directory";
		case CW_CALL_NO:
			return "Bad call to cashgettools function; may be bad implementation";
		case CW_SYS_ERR:
			return "There was an unexpected system error. This may be problem with cashgettools";	
		case CWG_CALL_ID_NO:
			return "Invalid identifier queried";
		case CWG_IN_DIR_NO:
			return "Requested file doesn't exist in specified directory";
		case CWG_IS_DIR_NO:
			return "Requested file is not a valid directory index, or contains invalid reference for requested path";
		case CWG_FETCH_NO:
			return "Requested file doesn't exist, check identifier";
		case CWG_METADATA_NO:
			return "Requested file's metadata is invalid or nonexistent, check identifier";	
		case CWG_CIRCLEREF_NO:
			return "Requested file contains a circular reference (invalid scripting or directory structure)";
		case CWG_FETCH_ERR:
			return "There was an unexpected error in querying the blockchain";
		case CWG_WRITE_ERR:
			return "There was an unexpected error in writing the file";
		case CWG_FILE_LEN_ERR:
		case CWG_FILE_DEPTH_ERR:
		case CWG_FILE_ERR:
			return "There was an unexpected error in interpreting the file. The file may be encoded incorrectly (i.e. inaccurate metadata/structuring), or there is a problem with cashgettools";
		case CWG_SCRIPT_RETRY_ERR:
		case CWG_SCRIPT_ERR:
			return "Requested nametag's encoded script is either invalid or lacks a file reference";
		case CWG_SCRIPT_REV_NO:
		case CWG_SCRIPT_NO:
		default:
			return "Unexpected error code. This is likely an issue with cashgettools";
	}
}

/* ---------------------------------------------------------------------------------- */

static inline void init_CWG_script_pack(struct CWG_script_pack *sp, List *scriptStreams, List *fetchedNames, const char *revTxid, int maxRev) {
	sp->scriptStreams = scriptStreams;
	sp->fetchedNames = fetchedNames;
	sp->revTxid = revTxid;
	sp->revTxidFirst = revTxid;
	sp->atRev = 0;
	sp->maxRev = maxRev;
	sp->infoCounter = NULL;
}

static inline void copy_inc_CWG_script_pack(struct CWG_script_pack *dest, struct CWG_script_pack *source) {
	dest->scriptStreams = source->scriptStreams;
	dest->fetchedNames = source->fetchedNames;
	dest->revTxid = source->revTxid;
	dest->revTxidFirst = source->revTxidFirst;
	dest->atRev = source->atRev+1;
	dest->maxRev = source->maxRev;
	dest->infoCounter = source->infoCounter;
}

static inline void init_CWG_nametag_counter(struct CWG_nametag_counter *cnc) {
	cnc->revisionTxid = NULL;
	cnc->revision = 0;
	initList(&cnc->nameRefs);
	initList(&cnc->txidRefs);
}

static inline void destroy_CWG_nametag_counter(struct CWG_nametag_counter *cnc) {
	if (cnc->revisionTxid) { free(cnc->revisionTxid); }
	removeAllNodes(&cnc->nameRefs, true);
	removeAllNodes(&cnc->txidRefs, true);
	init_CWG_nametag_counter(cnc);
}

static bool counter_copy_CWG_nametag_info(struct CWG_nametag_info *cni, struct CWG_nametag_counter *cnc) {
	cni->revisionTxid = cnc->revisionTxid; cnc->revisionTxid = NULL;
	cni->revision = cnc->revision;

	size_t i;
	size_t nameRefsCount = listLength(&cnc->nameRefs);
	if ((cni->nameRefs = malloc((nameRefsCount+1)*sizeof(char *))) == NULL) { perror("malloc failed"); return false; }
	for (i=0; i<nameRefsCount; i++) {
		if (((cni->nameRefs)[i] = popFront(&cnc->nameRefs)) == NULL) {
			// list will be NULL-terminated in this case, so cleanup can be handled normally
			fprintf(CWG_err_stream, "incorrect listLength() calculation; problem with cashgettools\n");
			return false;
		}
	}
	(cni->nameRefs)[i] = NULL;

	size_t txidRefsCount = listLength(&cnc->txidRefs);
	if ((cni->txidRefs = malloc((txidRefsCount+1)*sizeof(char *))) == NULL) { perror("malloc failed"); return false; }
	for (i=0; i<txidRefsCount; i++) {
		if (((cni->txidRefs)[i] = popFront(&cnc->txidRefs)) == NULL) {
			// list will be NULL-terminated in this case, so cleanup can be handled normally
			fprintf(CWG_err_stream, "incorrect listLength() calculation; problem with cashgettools\n");
			return false;
		}
	}
	(cni->txidRefs)[i] = NULL;

	return true;
}

static void protocolCheck(uint16_t pVer) {
	if (pVer > CW_P_VER) {
		fprintf(CWG_err_stream, "WARNING: requested file signals a newer cashweb protocol version than this client uses (client: CWP %u, file: CWP %u).\nWill attempt to read anyway, in case this is inaccurate or the protocol upgrade is trivial.\nIf there is a new protocol version available, it is recommended you upgrade.\n", CW_P_VER, pVer);
	}
}

static CW_STATUS cwTypeToMimeStr(CW_TYPE cwType, struct CWG_params *cgp) {
	if (!cgp->saveMimeStr) { return CW_OK; }
	(*cgp->saveMimeStr)[0] = 0;
	if (cwType <= CW_T_MIMESET) { return CW_OK; }

	CW_STATUS status = CW_OK;

	// determine mime.types full path by cashweb protocol version and set datadir path
	int dataDirPathLen = strlen(cgp->datadir);
	bool appendSlash = cgp->datadir[dataDirPathLen-1] != '/';
	char mtFilePath[dataDirPathLen + 1 + strlen(CW_DATADIR_MIMETYPES_PATH) + strlen("CW65535_mime.types") + 1];
	snprintf(mtFilePath, sizeof(mtFilePath), "%s%s%sCW%u_mime.types", cgp->datadir, appendSlash ? "/" : "", CW_DATADIR_MIMETYPES_PATH, CW_P_VER);

	// initialize data/file pointers before goto statements
	FILE *mimeTypes = NULL;
	struct DynamicMemory line;
	initDynamicMemory(&line);

	// checks for mime.types in data directory
	if (access(mtFilePath, F_OK) == -1) {
		status = CW_DATADIR_NO;
		goto cleanup;
	}

	// open protocol-specific mime.types file
	if ((mimeTypes = fopen(mtFilePath, "r")) == NULL) {
		fprintf(CWG_err_stream, "fopen() failed on path %s; unable to open cashweb mime.types: %s\n", mtFilePath, strerror(errno));
		status = CW_SYS_ERR;
		goto cleanup;
	}	

	// read mime.types file until appropriate line
	bool matched = false;
	bool mimeFileBad = false;
	char *lineDataPtr;
	CW_TYPE type = CW_T_MIMESET;	
	int readlineStatus;
	while ((readlineStatus = safeReadLine(&line, LINE_BUF, mimeTypes)) == READLINE_OK) {
		if (line.data[0] == '#') { continue; }
		if (++type != cwType) { continue; }

		if ((lineDataPtr = strchr(line.data, '\t')) == NULL) {
			fprintf(CWG_err_stream, "unable to parse for mimetype string, mime.types may be invalid; defaults to cashgettools MIME_STR_DEFAULT\n");
			mimeFileBad = true;
			break;

		}

		lineDataPtr[0] = 0;
		strcat(*cgp->saveMimeStr, line.data);
		matched = true;
	}
	if (ferror(mimeTypes)) { perror("fgets() failed on mime.types"); status = CW_SYS_ERR; goto cleanup; }
	else if (readlineStatus == READLINE_ERR) { status = CW_SYS_ERR; goto cleanup; }

	// defaults to MIME_STR_DEFAULT if type not found
	if (!matched) {
		if (!mimeFileBad) {
			fprintf(CWG_err_stream, "invalid cashweb type (numeric %u); defaults to MIME_STR_DEFAULT\n", cwType);
		}
	}

	cleanup:
		freeDynamicMemory(&line);
		if (mimeTypes) { fclose(mimeTypes); }
		return status;
}

static CW_STATUS writeHexDataStr(const char *hexDataStr, int suffixLen, int fd) {
	char fileByteData[strlen(hexDataStr)/2];
	int bytesToWrite;

	if ((bytesToWrite = hexStrToByteArr(hexDataStr, suffixLen, fileByteData)) < 0) {
		return CWG_FILE_ERR;
	}	

	ssize_t n;
	if ((n = write(fd, fileByteData, (size_t)bytesToWrite)) < bytesToWrite) {
		if (n < 0) { perror("write() failed"); }
		return CWG_WRITE_ERR;
	}
	
	return CW_OK;
}

static CW_STATUS hexResolveMetadata(const char *hexData, struct CW_file_metadata *md) {
	int hexDataLen = strlen(hexData);
	if (hexDataLen < CW_METADATA_CHARS) { return CWG_METADATA_NO; }
	const char *metadataPtr = hexData + hexDataLen - CW_METADATA_CHARS;

	int lengthHexLen = CW_MD_CHARS(length);
	int depthHexLen = CW_MD_CHARS(depth);
	int typeHexLen = CW_MD_CHARS(type);
	int pVerHexLen = CW_MD_CHARS(pVer);

	char chainLenHex[lengthHexLen+1]; chainLenHex[0] = 0;
	char treeDepthHex[depthHexLen+1]; treeDepthHex[0] = 0;
	char fTypeHex[typeHexLen+1]; fTypeHex[0] = 0;
	char pVerHex[pVerHexLen+1]; pVerHex[0] = 0;
	strncat(chainLenHex, metadataPtr, lengthHexLen); metadataPtr += lengthHexLen;
	strncat(treeDepthHex, metadataPtr, depthHexLen); metadataPtr += depthHexLen;
	strncat(fTypeHex, metadataPtr, typeHexLen); metadataPtr += typeHexLen;
	strncat(pVerHex, metadataPtr, pVerHexLen); metadataPtr += pVerHexLen;

	if (!netHexStrToInt(chainLenHex, CW_MD_BYTES(length), &md->length) ||
	    !netHexStrToInt(treeDepthHex, CW_MD_BYTES(depth), &md->depth) ||
	    !netHexStrToInt(fTypeHex, CW_MD_BYTES(type), &md->type) ||
	    !netHexStrToInt(pVerHex, CW_MD_BYTES(pVer), &md->pVer)) { return CWG_METADATA_NO; }

	return CW_OK;
}

static CW_STATUS traverseFileTree(const char *treeHexData, List *partialTxids[], int suffixLen, int depth,
			    	  struct CWG_params *params, struct CW_file_metadata *md, int fd) {
	char *partialTxid;
	size_t partialTxidFill = partialTxids != NULL && (partialTxid = popFront(partialTxids[0])) != NULL ?
			      	 CW_TXID_CHARS-strlen(partialTxid) : 0;	
	
	size_t numChars = strlen(treeHexData+partialTxidFill)-suffixLen;
	size_t txidsCount = numChars/CW_TXID_CHARS + (partialTxidFill ? 1 : 0);
	if (!txidsCount && depth) { return CWG_FILE_ERR; }

	CW_STATUS status = CW_OK;

	char *txids[txidsCount ? txidsCount : 1];
	for (int i=0; i<txidsCount; i++) {
		if (status == CW_SYS_ERR) { txids[i] = NULL; } 
		else if ((txids[i] = malloc(CW_TXID_CHARS+1)) == NULL) { perror("malloc failed"); status  = CW_SYS_ERR; }
	}
	if (status != CW_OK) { goto cleanup; }

	if (partialTxidFill && txidsCount) {
		strcpy(txids[0], partialTxid);
		strncat(txids[0], treeHexData, partialTxidFill);
		free(partialTxid);
	}
	const char *txidPtr = treeHexData+partialTxidFill;
	size_t sTxidsCount = (partialTxidFill ? 1 : 0);
	if (txidsCount < sTxidsCount) { status = CWG_FILE_ERR; goto cleanup; }
	for (int i=sTxidsCount; i<txidsCount; i++) {
		strncpy(txids[i], txidPtr, CW_TXID_CHARS);
		txids[i][CW_TXID_CHARS] = 0;
		txidPtr += CW_TXID_CHARS;
	}
	char *partialTxidN = malloc(CW_TXID_CHARS+1);
	if (!partialTxidN) { perror("malloc failed"); status = CW_SYS_ERR; goto cleanup; }
	partialTxidN[0] = 0; strncat(partialTxidN, txidPtr, numChars-(txidPtr-(treeHexData+partialTxidFill)));

	if (!txidsCount) {
		if (!addFront(partialTxids[0], partialTxidN)) {
			perror("mylist addFront() failed");
			free(partialTxidN);
			status = CW_SYS_ERR;
		}
		goto cleanup;
	}

	// frees the tree hex data if in recursive call
	if (depth > 0) { free((char *)treeHexData); }

	char *hexDataAll = malloc(CW_TX_DATA_CHARS*txidsCount + 1);
	if (hexDataAll == NULL) { perror("malloc failed"); status =  CW_SYS_ERR; goto cleanup; }
	if ((status = fetchHexData((const char **)txids, txidsCount, BY_TXID, params, NULL, hexDataAll)) == CW_OK) { 
		if (partialTxids != NULL) {
			if (!addFront(partialTxids[1], partialTxidN)) {
				perror("mylist addFront() failed");
				free(partialTxidN);
				free(hexDataAll);
				status = CW_SYS_ERR;
				goto cleanup;
			}
		} else { free(partialTxidN); } 

		if (depth+1 < md->depth) {
			status = traverseFileTree(hexDataAll, partialTxids, 0, depth+1, params, md, fd);
		} else {
			status = writeHexDataStr(hexDataAll, 0, fd);
			free(hexDataAll);
			if (status != CW_OK) { goto cleanup; }

			if (partialTxids != NULL) {
				reverseList(partialTxids[1]);
				List *temp = partialTxids[0];
				partialTxids[0] = partialTxids[1];
				partialTxids[1] = temp;
			}
		}
	} else if (status == CWG_FETCH_NO) { status = CWG_FILE_DEPTH_ERR; }

	cleanup:
		for (int i=0; i<txidsCount; i++) { if (txids[i]) { free(txids[i]); } }
		return status;
}

static CW_STATUS traverseFileChain(const char *hexDataStart, struct CWG_params *params, struct CW_file_metadata *md, int fd) {
	char hexData[CW_TX_DATA_CHARS+1];
	strcpy(hexData, hexDataStart);
	char *hexDataNext = malloc(CW_TX_DATA_CHARS+1);
	if (hexDataNext == NULL) { perror("malloc failed"); return CW_SYS_ERR; }
	char *txidNext = malloc(CW_TXID_CHARS+1);
	if (txidNext == NULL) { perror("malloc failed"); free(hexDataNext); return CW_SYS_ERR; }

	List partialTxidsO; List partialTxidsN;
	List *partialTxids[2] = { &partialTxidsO, &partialTxidsN };
	initList(partialTxids[0]); initList(partialTxids[1]);

	CW_STATUS status;
	int suffixLen;
	bool end = false;
	for (int i=0; i <= md->length; i++) {
		if (i == 0) {
			suffixLen = CW_METADATA_CHARS;
			if (i < md->length) { suffixLen += CW_TXID_CHARS; } else { end = true; }
		}
		else if (i == md->length) {
			suffixLen = 0;
			end = true;
		}
		else {
			suffixLen = CW_TXID_CHARS;
		}
	
		if (strlen(hexData) < suffixLen) { status = CWG_FILE_ERR; goto cleanup; }
		if (!end) {
			strncpy(txidNext, hexData+(strlen(hexData) - suffixLen), CW_TXID_CHARS);
			txidNext[CW_TXID_CHARS] = 0;
			if ((status = fetchHexData((const char **)&txidNext, 1, BY_TXID, params, NULL, hexDataNext)) == CWG_FETCH_NO) {
				status = CWG_FILE_LEN_ERR;	
				goto cleanup;
			} else if (status != CW_OK) { goto cleanup; }
		}

		if (!md->depth) {
			if ((status = writeHexDataStr(hexData, suffixLen, fd)) != CW_OK) { goto cleanup; }
		} else {
			if ((status = traverseFileTree(hexData, partialTxids, suffixLen, 0, params, md, fd)) != CW_OK) {
				goto cleanup;
			}
		} 
		memcpy(hexData, hexDataNext, sizeof(hexData));
	}
	
	cleanup:
		removeAllNodes(partialTxids[0], true); removeAllNodes(partialTxids[1], true);
		free(txidNext);
		free(hexDataNext);
		return status;
}

static inline CW_STATUS traverseFile(const char *hexDataStart, struct CWG_params *params, struct CW_file_metadata *md, int fd) {
	return md->length > 0 || md->depth == 0 ? traverseFileChain(hexDataStart, params, md, fd)
						: traverseFileTree(hexDataStart, NULL, CW_METADATA_CHARS, 0, params, md, fd);
}

static inline void freeFdStack(List *fdStack) {
	int fd;
	int *fdPtr;
	while ((fdPtr = popFront(fdStack))) {
		fd = *fdPtr;	
		free(fdPtr);
		if (fcntl(fd, F_GETFD) != -1) { close(fd); }
	}
}

static inline CW_STATUS writePathLink(const char *pathR, const char *linkR, int fd) {
	const char *path = pathR[0] == '/' ? pathR+1 : pathR;
	const char *link = linkR[0] == '/' ? linkR+1 : linkR;
	size_t pathLen = strlen(path);
	size_t linkLen = strlen(link);

	ssize_t n;
	if ((n = write(fd, "/", 1)) < 1 ||
	    (n = write(fd, path, pathLen)) < pathLen ||
	    (n = write(fd, "\n", 1)) < 1 ||
	    (n = write(fd, "./", 2)) < 2 ||
	    (n = write(fd, link, linkLen)) < linkLen ||
	    (n = write(fd, "\n", 1)) < 1) {
		if (n < 0) { perror("write() failed"); }
		return CWG_WRITE_ERR;
	}

	return CW_OK;
}

static CW_STATUS execScriptCode(CW_OPCODE c, FILE *scriptStream, List *stack, List *fdStack, struct CWG_script_pack *sp, struct CWG_params *params, int fd) {
	switch (c) {
		case CW_OP_TERM:
			return CWG_SCRIPT_NO;
		case CW_OP_NEXTREV:
		{
			if (sp->maxRev >= 0 && sp->atRev >= sp->maxRev) { return CWG_SCRIPT_REV_NO; }

			struct CWG_script_pack spN;
			copy_inc_CWG_script_pack(&spN, sp);

			FILE *nextScriptStream = NULL;
			if (sp->revTxid) {
				if ((nextScriptStream = tmpfile()) == NULL) { perror("tmpfile() failed"); return CW_SYS_ERR; }

				char nextRevTxid[CW_TXID_CHARS+1]; char *nextRevTxidPtr = nextRevTxid;

				CW_STATUS status;
				if ((status = getScriptByInTxid(sp->revTxid, params, &nextRevTxidPtr, nextScriptStream)) != CW_OK) {
					fclose(nextScriptStream);
					if (status == CWG_FETCH_NO) { return CWG_SCRIPT_REV_NO; }
					return status;
				}
				rewind(nextScriptStream);

				if (!addFront(sp->scriptStreams, nextScriptStream)) { perror("mylist addFront() failed"); fclose(nextScriptStream); return CW_SYS_ERR; }

				spN.revTxid = nextRevTxid;
				if (sp->infoCounter) { sp->infoCounter->revision = spN.atRev; }
			}

			CW_STATUS status = execScript(&spN, params, fd);

			if (nextScriptStream) { fclose(nextScriptStream); }
			return status;
		}
		case CW_OP_PUSHTXID:
		{
			char txidBytes[CW_TXID_BYTES];
			if (fread(txidBytes, 1, CW_TXID_BYTES, scriptStream) < CW_TXID_BYTES) {
				if (ferror(scriptStream)) { perror("fread() failed on scriptStream"); return CW_SYS_ERR; }
				return CWG_SCRIPT_ERR;
			}

			char *txid = malloc(CW_TXID_CHARS+1);
			if (txid == NULL) { perror("malloc failed"); return CW_SYS_ERR; }
			byteArrToHexStr(txidBytes, CW_TXID_BYTES, txid);

			if (!addFront(stack, txid)) { perror("mylist addFront() failed"); free(txid); return CW_SYS_ERR; }
			return CW_OK;
		}
		case CW_OP_WRITEFROMTXID:
		{
			char *txid;
			if ((txid = popFront(stack)) == NULL) { return CWG_SCRIPT_ERR; }
			else if (!CW_is_valid_txid(txid)) { free(txid); return CWG_SCRIPT_ERR; }

			if (sp->infoCounter) {
				if (!addFront(&sp->infoCounter->txidRefs, txid)) { perror("mylist addFront() failed"); free(txid); return CW_SYS_ERR; }
				return CW_OK;
			}
			CW_STATUS status = getFileByTxid(txid, sp->fetchedNames, params, NULL, fd);

			free(txid);
			if (status == CWG_FETCH_NO) { return CWG_SCRIPT_ERR; }
			return status;
		}
		case CW_OP_WRITEFROMNAMETAG:
		{
			char *name;
			if ((name = popFront(stack)) == NULL || !CW_is_valid_name(name)) {
				if (name) { free(name); }
				return CWG_SCRIPT_ERR;
			}

			if (sp->infoCounter) {
				if (!addFront(&sp->infoCounter->nameRefs, name)) { perror("mylist addFront() failed"); free(name); return CW_SYS_ERR; }
				return CW_OK;
			}
			CW_STATUS status = getFileByNametag(name, CW_REV_LATEST, sp->fetchedNames, params, NULL, fd);

			free(name);
			if (status == CWG_FETCH_NO || status == CW_CALL_NO) { return CWG_SCRIPT_ERR; }
			return status;
		}
		case CW_OP_WRITEFROMPREV:
		{
			if (sp->atRev < 1) { return CWG_SCRIPT_ERR; }

			CW_STATUS status = CW_OK;
			
			List scriptStreams;
			List scriptStreamsSavePos;
			initList(&scriptStreams);
			initList(&scriptStreamsSavePos);

			FILE *scriptStream;
			long *savePosPtr;
			Node *savePosNodeLast = NULL;

			Node *n = sp->scriptStreams->head;
			while (n) {
				scriptStream = n->data;

				if ((savePosPtr = malloc(sizeof(long))) == NULL) { perror("malloc failed"); status = CW_SYS_ERR; break; }
				*savePosPtr = ftell(scriptStream);
				if (*savePosPtr < 0) { perror("ftell() failed on scriptStream"); free(savePosPtr); status = CW_SYS_ERR; break; }
				if ((savePosNodeLast = addAfter(&scriptStreamsSavePos, savePosNodeLast, savePosPtr)) == NULL) {
					perror("mylist addAfter() failed");
					free(savePosPtr);
					status = CW_SYS_ERR;
					break;
				}

				if (!addFront(&scriptStreams, scriptStream)) { perror("mylist addFront() failed"); status = CW_SYS_ERR; break;  }
				rewind(scriptStream);

				n = n->next;
			}
			if (status == CW_OK && isEmptyList(&scriptStreams)) {
				fprintf(CWG_err_stream, "scriptStreams empty in execScriptCode(); problem with cashgettools\n");
				status = CW_SYS_ERR;
			}
			if (status != CW_OK) {
				removeAllNodes(&scriptStreamsSavePos, true);
				removeAllNodes(&scriptStreams, false);
				return status;
			}

			if (sp->revTxid == NULL) { reverseList(&scriptStreams); }

			struct CWG_script_pack spD;
			init_CWG_script_pack(&spD, &scriptStreams, sp->fetchedNames, NULL, sp->atRev-1);
			spD.infoCounter = sp->infoCounter;

			status = execScriptStart(&spD, params, fd);

			n = sp->scriptStreams->head;
			while (n) {
				scriptStream = n->data;

				if ((savePosPtr = popFront(&scriptStreamsSavePos)) == NULL) {
					fprintf(CWG_err_stream, "scriptStreamsSavePos invalid size in execScriptCode(); problem with cashgettools\n");
					status = CW_SYS_ERR;
					break;
				}
				if (fseek(scriptStream, *savePosPtr, SEEK_SET) < 0) { perror("fseek() failed on scriptStream"); status = CW_SYS_ERR; }
				free(savePosPtr);
				if (status != CW_OK) { break; }
				
				n = n->next;
			}

			removeAllNodes(&scriptStreamsSavePos, true);
			removeAllNodes(&scriptStreams, false);	
			return status;
		}
		case CW_OP_PUSHCHAR:
		case CW_OP_PUSHSHORT:
		case CW_OP_PUSHINT:
		{
			int numBytes;
			switch (c) {
				case CW_OP_PUSHCHAR:
					numBytes = sizeof(uint8_t);
					break;
				case CW_OP_PUSHSHORT:
					numBytes = sizeof(uint16_t);
					break;
				default:
					numBytes = sizeof(uint32_t);
					break;
			}

			char intBytes[numBytes];
			if (fread(intBytes, 1, sizeof(intBytes), scriptStream) < sizeof(intBytes)) {
				if (ferror(scriptStream)) { perror("fread() failed on scriptStream"); return CW_SYS_ERR; }
				return CWG_SCRIPT_ERR;
			}

			char *hexStr = malloc((sizeof(intBytes)*2)+1);
			if (hexStr == NULL) { perror("malloc failed"); return CW_SYS_ERR; }
			byteArrToHexStr(intBytes, sizeof(intBytes), hexStr);

			if (!addFront(stack, hexStr)) { perror("mylist addFront() failed"); free(hexStr); return CW_SYS_ERR; }
			return CW_OK;
		}
		case CW_OP_STOREFROMTXID:
		case CW_OP_STOREFROMNAMETAG:
		case CW_OP_STOREFROMPREV:
		{
			char tmpname[] = "/tmp/CWscriptstore-XXXXXX";
			int tfd = mkstemp(tmpname);
			unlink(tmpname);
			if (tfd < 0) { perror("mkstemp() failed"); return CW_SYS_ERR; }

			CW_OPCODE writeOp;
			switch (c) {
				case CW_OP_STOREFROMTXID:
					writeOp = CW_OP_WRITEFROMTXID;
					break;
				case CW_OP_STOREFROMNAMETAG:
					writeOp = CW_OP_WRITEFROMNAMETAG;
					break;
				default:
					writeOp = CW_OP_WRITEFROMPREV;
					break;
			}

			CW_STATUS status;		
			void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
			params->foundHandler = NULL;
			status = execScriptCode(writeOp, scriptStream, stack, fdStack, sp, params, tfd);
			params->foundHandler = savePtr;
			if (status != CW_OK)  { close(tfd); return status; }	

			if (lseek(tfd, 0, SEEK_SET) < 0) { perror("lseek() failed SEEK_SET"); close(tfd); return CW_SYS_ERR; }

			int *tfdHeap = malloc(sizeof(int));
			if (!tfdHeap) { perror("malloc failed"); close(tfd); return CW_SYS_ERR; }
			*tfdHeap = tfd;
			if (!addFront(fdStack, tfdHeap)) { perror("mylist addFront() failed"); close(tfd); return CW_SYS_ERR; }

			return CW_OK;
		}
		case CW_OP_SEEKSTORED:
		{
			CW_STATUS status = CW_OK;
			int offset;
			int whence;
			int tfd;

			char *offsetHexStr = popFront(stack);
			if (!offsetHexStr) { return CWG_SCRIPT_ERR; }
			size_t offsetBytes = strlen(offsetHexStr)/2;

			uint32_t offsetU = 0;
			if (offsetBytes == sizeof(uint8_t)) {  offsetU = (uint32_t)strtoul(offsetHexStr, NULL, 16); }
			else if (offsetBytes == sizeof(uint16_t) || offsetBytes == sizeof(uint32_t)) {
				if (!netHexStrToInt(offsetHexStr, offsetBytes, &offsetU)) { status = CW_SYS_ERR; }
			}
			else { status = CWG_SCRIPT_ERR; }
			free(offsetHexStr);
			if (status != CW_OK) { return status; }
			
			if (offsetU > INT_MAX) { return CWG_SCRIPT_ERR; }
			offset = (int)offsetU;

			char *whenceHexStr = popFront(stack);
			if (!whenceHexStr) { return CWG_SCRIPT_ERR; }
			size_t whenceBytes = strlen(whenceHexStr)/2;
			uint8_t cwWhence = 0;
			if (whenceBytes == sizeof(uint8_t)) { cwWhence = (uint8_t)strtoul(whenceHexStr, NULL, 16); } 
			else { status = CWG_SCRIPT_ERR; }
			free(whenceHexStr);
			if (status != CW_OK) { return status; }

			switch (cwWhence) {
				case CW_SEEK_BEG:
					whence = SEEK_SET;
					break;
				case CW_SEEK_CUR:
					whence = SEEK_CUR;
					break;
				case CW_SEEK_CUR_NEG:
					whence = SEEK_CUR;
					offset *= -1;
					break;
				case CW_SEEK_END_NEG:
					whence = SEEK_END;
					offset *= -1;
					break;
				default:
					return CWG_SCRIPT_ERR;
			}

			int *tfdPtr = peekFront(fdStack);	
			if (!tfdPtr) { return CWG_SCRIPT_ERR; }
			tfd = *tfdPtr;
			if (fcntl(tfd, F_GETFD) == -1) { fprintf(CWG_err_stream, "invalid fildes during script execution; problem with cashgettools\n"); return CW_SYS_ERR; }
			if (lseek(tfd, offset, whence) < 0) { return CWG_SCRIPT_ERR; }

			return CW_OK;
		}
		case CW_OP_WRITEFROMSTORED:
		case CW_OP_WRITESOMEFROMSTORED:
		{
			CW_STATUS status = CW_OK;
			uint32_t some = 0;
			int tfd;

			bool writeAll = false;
			if (c == CW_OP_WRITESOMEFROMSTORED) {
				char *someHexStr = popFront(stack);
				if (!someHexStr) { return CWG_SCRIPT_ERR; }
				size_t someBytes = strlen(someHexStr)/2;

				if (someBytes == sizeof(uint8_t)) {  some = (uint32_t)strtoul(someHexStr, NULL, 16); }
				else if (someBytes == sizeof(uint16_t) || someBytes == sizeof(uint32_t)) {
					if (!netHexStrToInt(someHexStr, someBytes, &some)) { status = CW_SYS_ERR; }
				}
				else { status = CWG_SCRIPT_ERR; }
				free(someHexStr);
				if (status != CW_OK) { return status; }
			} else { writeAll = true; }

			if (sp->infoCounter) { return CW_OK; }

			int *tfdPtr = peekFront(fdStack);
			if (!tfdPtr) { return CWG_SCRIPT_ERR; }
			tfd = *tfdPtr;

			if (fcntl(tfd, F_GETFD) == -1) { fprintf(CWG_err_stream, "invalid fildes during script execution; problem with cashgettools\n"); return CW_SYS_ERR; }

			size_t toWrite = (size_t)some;
			if (toWrite == 0 && !writeAll) { return CWG_SCRIPT_ERR; }

			if (params->foundHandler != NULL) { params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL; }

			char buf[FILE_DATA_BUF];
			size_t maxWriteChunk = toWrite < sizeof(buf) && !writeAll ? toWrite : sizeof(buf);
			size_t writeChunk = toWrite < maxWriteChunk && !writeAll ? toWrite : maxWriteChunk;
			ssize_t r;
			ssize_t w;
			while ((toWrite > 0 || writeAll) && (r = read(tfd, buf, writeChunk)) > 0) {
				if ((w = write(fd, buf, r)) < r) {
					if (w < 0) { perror("write() failed"); }
					return CWG_WRITE_ERR;
				}
				if (!writeAll) {
					toWrite -= w;
					writeChunk = toWrite < maxWriteChunk ? toWrite : maxWriteChunk;
				}
			}
			if (r < 0) { perror("read() failed"); return CW_SYS_ERR; }
			if (toWrite > 0) { return CWG_SCRIPT_ERR; }

			return CW_OK;
		}
		case CW_OP_DROPSTORED:
		{
			int *tfdHeap = popFront(fdStack);
			if (!tfdHeap) { return CWG_SCRIPT_ERR; }
			int tfd = *tfdHeap;
			free(tfdHeap);

			if (fcntl(tfd, F_GETFD) == -1) { return CWG_SCRIPT_ERR; }
			if (lseek(tfd, 0, SEEK_SET) < 0) { return CWG_SCRIPT_ERR; } // in this case, presumed to not be file descriptor from mkstemp()

			close(tfd);
			return CW_OK;
		}
		case CW_OP_WRITEPATHLINK:
		{
			char *pathS;
			char *linkS;
			if ((pathS = popFront(stack)) == NULL) { return CWG_SCRIPT_ERR; }
			if ((linkS = popFront(stack)) == NULL) { free(pathS); return CWG_SCRIPT_ERR; }	

			CW_STATUS status = CW_OK;

			if (!params->foundHandler) { status = writePathLink(pathS, linkS, fd); }

			free(linkS);
			free(pathS);	
			return status;
		}
		case CW_OP_PUSHSTRX:	
		default:
		{
			CW_STATUS status = CW_OK;

			size_t len;
			if (c == CW_OP_PUSHSTRX) {
				char *pushLenHexStr = popFront(stack);
				if (!pushLenHexStr) { return CWG_SCRIPT_ERR; }
				size_t pushLenBytes = strlen(pushLenHexStr)/2;

				uint32_t pushLen;
				if (pushLenBytes == sizeof(uint8_t)) {  pushLen = (uint32_t)strtoul(pushLenHexStr, NULL, 16); }
				else if (pushLenBytes == sizeof(uint16_t) || pushLenBytes == sizeof(uint32_t)) {
					if (!netHexStrToInt(pushLenHexStr, pushLenBytes, &pushLen)) { status = CW_SYS_ERR; }
				}
				else { status = CWG_SCRIPT_ERR; }
				free(pushLenHexStr);
				if (status != CW_OK) { return status; }

				len = (size_t)pushLen;
			}
			else if (c > CW_OP_PUSHSTR) { return CWG_SCRIPT_ERR; }
			else if (c == CW_OP_PUSHNO) { return CW_OK; }
			else { len = (size_t)c; }

			char *pushStr = malloc(len+1);	
			if (pushStr == NULL) { perror("malloc failed"); }	
			if (fread(pushStr, 1, len, scriptStream) < len) {
				if (ferror(scriptStream)) { perror("fread() failed on scriptStream"); free(pushStr); return CW_SYS_ERR; }
				return CWG_SCRIPT_ERR;
			}
			for (int i=0; i<len; i++) { if (pushStr[i] == 0) { free(pushStr); return CWG_SCRIPT_ERR; } }
			pushStr[len] = 0;

			if (!addFront(stack, pushStr)) { perror("mylist addFront() failed"); free(pushStr); return CW_SYS_ERR; }
			return CW_OK;
		}
	}
}

static CW_STATUS execScript(struct CWG_script_pack *sp, struct CWG_params *params, int fd) {
	FILE *scriptStream;
	if (sp->revTxid) {
		if ((scriptStream = peekFront(sp->scriptStreams)) == NULL) {
			fprintf(CWG_err_stream, "mylist peekFront() failed on scriptStreams; problem with cashgettools\n");
			return CW_SYS_ERR;
		}
	} else {
		if ((scriptStream = peekAt(sp->scriptStreams, sp->atRev)) == NULL) {
			fprintf(CWG_err_stream, "mylist peekAt() failed on scriptStreams for atRev; problem with cashgettools\n");
			return CW_SYS_ERR;
		}
	}

	CW_STATUS status = CW_OK;

	List stack;
	initList(&stack);	

	List fdStack;
	initList(&fdStack);

	int c;	
	CW_OPCODE code;
	while ((c = getc(scriptStream)) != EOF) {
		code = (CW_OPCODE)c;
		// if script is invalid, will attempt to replace with next revision; if it isn't there, will return CWG_SCRIPT_ERR
		if ((status = execScriptCode(code, scriptStream, &stack, &fdStack, sp, params, fd)) == CWG_SCRIPT_ERR) {
			removeAllNodes(&stack, true);
			freeFdStack(&fdStack);
			
			if ((status = execScriptCode(CW_OP_NEXTREV, scriptStream, &stack, &fdStack, sp, params, fd)) == CWG_SCRIPT_REV_NO || status == CWG_SCRIPT_ERR) {
				status = CWG_SCRIPT_RETRY_ERR;
			}
			goto cleanup;
		}
		else if (status == CWG_SCRIPT_REV_NO) {
			if (sp->infoCounter && sp->revTxid) {
				if (!sp->infoCounter->revisionTxid && (sp->infoCounter->revisionTxid = strdup(sp->revTxid)) == NULL) {
					perror("strdup() failed");
					status = CW_SYS_ERR;
					goto cleanup;
				}
			}
			status = CW_OK;
			continue;
		}
		else if (status != CW_OK) { goto cleanup; }
	}
	if (ferror(scriptStream)) { perror("getc() failed on scriptStream"); status = CW_SYS_ERR; goto cleanup; }

	cleanup:	
		removeAllNodes(&stack, true);
		freeFdStack(&fdStack);
		if (sp->revTxid) { popFront(sp->scriptStreams); }
		return status;
}

static inline CW_STATUS execScriptStart(struct CWG_script_pack *sp, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = execScript(sp, params, fd)) == CWG_SCRIPT_NO) { status = CW_OK; }
	return status;
}

static CW_STATUS getScriptByInTxid(const char *inTxid, struct CWG_params *params, char **txidPtr, FILE *stream) {
	CW_STATUS status;

	char hexDataStart[CW_TX_DATA_CHARS+1];
	struct CW_file_metadata md;

	if ((status = fetchHexData((const char **)&inTxid, 1, BY_INTXID, params, txidPtr, hexDataStart)) != CW_OK) { return status; }
	if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { return status; }
	protocolCheck(md.pVer);

	return traverseFile(hexDataStart, params, &md, fileno(stream));
}

static CW_STATUS getScriptByNametag(const char *name, struct CWG_params *params, char **txidPtr, FILE *stream) {
	if (!CW_is_valid_name(name)) {
		fprintf(CWG_err_stream, "cashgettools: nametag specified for get is too long (maximum %lu characters)\n", CW_NAME_MAX_LEN);
		return CW_CALL_NO;
	}

	CW_STATUS status;

	char hexDataStart[CW_TX_DATA_CHARS+1];
	struct CW_file_metadata md;

	char nametag[strlen(CW_NAMETAG_PREFIX) + strlen(name) + 1]; nametag[0] = 0;
	strcat(nametag, CW_NAMETAG_PREFIX);
	strcat(nametag, name);
	char *nametagPtr = nametag;

	// gets the nths occurrence of nametag; skips any claim that is invalid cashweb file (NOT invalid script) to avoid mistaken claims
	int nth = 1;
	do {
		if ((status = fetchHexData((const char **)&nametagPtr, nth++, BY_NAMETAG, params, txidPtr, hexDataStart)) != CW_OK) { continue; }
		if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { continue; }
		protocolCheck(md.pVer);
		status = traverseFile(hexDataStart, params, &md, fileno(stream));
	} while (status == CWG_FILE_ERR || status == CWG_METADATA_NO);

	return status;
}

static CW_STATUS getFileByPath(FILE *dirFp, const char *path, List *fetchedNames, struct CWG_params *params, int fd) {
	CW_STATUS status;	

	char *pathId = NULL;
	char *subPath = NULL;
	if ((status = CWG_dirindex_path_to_identifier(dirFp, path, &subPath, &pathId)) != CW_OK) { goto foundhandler; }	

	if ((status = getFileByIdPath(pathId, subPath, fetchedNames, params, fd)) == CW_CALL_NO || status == CWG_FETCH_NO) { status = CWG_IS_DIR_NO; }

	foundhandler:
	if (params->foundHandler != NULL) {
		if (status == params->foundSuppressErr) { status = CW_OK; }
		params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL;
	}

	if (subPath) { free(subPath); }
	if (pathId) { free(pathId); }
	return status;	
}

static inline CW_STATUS getFileByNametagPath(const char *name, int revision, const char *path, List *fetchedNames, struct CWG_params *params, int fd) {	
	struct CWG_getter getter;
	init_CWG_getter_for_name(&getter, name, revision, fetchedNames, params);
	return getByGetterPath(&getter, path, fd);
}

static CW_STATUS getFileByNametag(const char *name, int revision, List *fetchedNames, struct CWG_params *params, struct CWG_nametag_counter *counter, int fd) {	
	CW_STATUS status;	

	char revTxid[CW_TXID_CHARS+1]; char *revTxidPtr = revTxid;
	FILE *scriptStream = NULL;

	List scriptStreams;
	initList(&scriptStreams);

	List fetchedNamesN;
	initList(&fetchedNamesN);

	// check for circular reference in fetched names
	if (fetchedNames) {
		Node *n = fetchedNames->head;
		while (n) {
			if (strcmp(name, n->data) == 0) { status = CWG_CIRCLEREF_NO; goto foundhandler; }
			n = n->next;
		}
	}

	if ((scriptStream = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto foundhandler; }	
	if ((status = getScriptByNametag(name, params, &revTxidPtr, scriptStream)) != CW_OK) { goto foundhandler; }
	rewind(scriptStream);	
	if (!addFront(&scriptStreams, scriptStream)) { perror("mylist addFront() failed"); status = CW_SYS_ERR; goto foundhandler; }

	struct CWG_script_pack sp;
	init_CWG_script_pack(&sp, &scriptStreams, fetchedNames ? fetchedNames : &fetchedNamesN, revTxid, revision);
	sp.infoCounter = counter;
	if (!addFront(sp.fetchedNames, (char *)name)) { perror("mylist addFront() failed"); status = CW_SYS_ERR; goto foundhandler; }
	
	status = execScriptStart(&sp, params, fd);	

	// this should have been set NULL if anything was written from script execution; if not, it's deemed a bad script
	if (status == CW_OK && params->foundHandler != NULL) { status = CWG_SCRIPT_ERR; }

	foundhandler:
	if (params->foundHandler != NULL) {
		if (status == params->foundSuppressErr) { status = CW_OK; }
		params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL;
	}

	removeAllNodes(&fetchedNamesN, false);
	removeAllNodes(&scriptStreams, false);
	if (scriptStream) { fclose(scriptStream); }
	return status;
}

static inline CW_STATUS getFileByTxidPath(const char *txid, const char *path, List *fetchedNames, struct CWG_params *params, int fd) {
	struct CWG_getter getter;
	init_CWG_getter_for_txid(&getter, txid, fetchedNames, params);
	return getByGetterPath(&getter, path, fd);
}

static CW_STATUS getFileByTxid(const char *txid, List *fetchedNames, struct CWG_params *params, struct CWG_file_info *counter, int fd) {
	CW_STATUS status;

	char hexDataStart[CW_TX_DATA_CHARS+1];
	struct CW_file_metadata md;

	if ((status = fetchHexData((const char **)&txid, 1, BY_TXID, params, NULL, hexDataStart)) != CW_OK) { goto foundhandler; }
	if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { goto foundhandler; }
	protocolCheck(md.pVer);	

	if (params->saveMimeStr && (*params->saveMimeStr)[0] == 0) {
		if ((status = cwTypeToMimeStr(md.type, params)) != CW_OK) { goto foundhandler; }
	}	

	if (params->forceDir && md.type != CW_T_DIR) { status = CWG_IS_DIR_NO; goto foundhandler; }

	foundhandler:
	if (params->foundHandler != NULL) {
		if (status == params->foundSuppressErr) { status = CW_OK; }
		params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL;
	}
	if (status != CW_OK) { return status; }

	if (counter) { copy_CW_file_metadata(&counter->metadata, &md); return CW_OK; }

	return traverseFile(hexDataStart, params, &md, fd);
}

static inline CW_STATUS getFileByIdPath(const char *id, const char *path, List *fetchedNames, struct CWG_params *params, int fd) {
	struct CWG_getter getter;
	init_CWG_getter_for_id(&getter, id, fetchedNames, params);
	return getByGetterPath(&getter, path, fd);
}

static CW_STATUS getFileById(const char *id, List *fetchedNames, struct CWG_params *params, int fd) {
	char idEnc[CW_NAMETAG_ID_MAX_LEN+1];
	const char *path;
	const char *name;
	int rev;

	CW_STATUS status;

	if (CW_is_valid_path_id(id, idEnc, &path)) { status = getFileByIdPath(idEnc, path, fetchedNames, params, fd); }
	else if (CW_is_valid_nametag_id(id, &rev, &name)) { status = getFileByNametag(name, rev, fetchedNames, params, NULL, fd); }	
	else if (CW_is_valid_txid(id)) { status = getFileByTxid(id, fetchedNames, params, NULL, fd); }
	else { status = CW_CALL_NO; goto foundhandler; }

	foundhandler:
	if (params->foundHandler != NULL) {
		if (status == params->foundSuppressErr) { status = CW_OK; }
		params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL;
	}

	return status;
}

static inline void init_CWG_getter_for_id(struct CWG_getter *cgg, const char *id, List *fetchedNames, struct CWG_params *params) {
	cgg->byId = &getFileById;
	cgg->byTxid = NULL;
	cgg->byName = NULL;
	cgg->id = id;
	cgg->name = NULL;
	cgg->revision = 0;
	cgg->fetchedNames = fetchedNames;
	cgg->params = params;
}

static inline void init_CWG_getter_for_txid(struct CWG_getter *cgg, const char *txid, List *fetchedNames, struct CWG_params *params) {
	cgg->byId = NULL;
	cgg->byTxid = &getFileByTxid;
	cgg->byName = NULL;
	cgg->id = txid;
	cgg->name = NULL;
	cgg->revision = 0;
	cgg->fetchedNames = fetchedNames;
	cgg->params = params;
}

static inline void init_CWG_getter_for_name(struct CWG_getter *cgg, const char *name, int revision, List *fetchedNames, struct CWG_params *params) {
	cgg->byId = NULL;
	cgg->byTxid = NULL;
	cgg->byName = &getFileByNametag;
	cgg->id = NULL;
	cgg->name = name;
	cgg->revision = revision;
	cgg->fetchedNames = fetchedNames;
	cgg->params = params;
}

static inline CW_STATUS getByGetter(struct CWG_getter *getter, int fd) {
	if (getter->byTxid) { return getter->byTxid(getter->id, getter->fetchedNames, getter->params, NULL, fd); }
	else if (getter->byName) { return getter->byName(getter->name, getter->revision, getter->fetchedNames, getter->params, NULL, fd); }
	else { return getter->byId(getter->id, getter->fetchedNames, getter->params, fd); }
}

static CW_STATUS getByGetterPath(struct CWG_getter *getter, const char *path, int fd) {
	CW_STATUS status;
	struct CWG_params *params = getter->params;

	if (path == NULL) {
		status = getByGetter(getter, fd);
		return status;
	}	

	FILE *dirFp = tmpfile();
	if (!dirFp) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto foundhandler; }

	bool saveBool = params->forceDir;
	params->forceDir = true;
	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	params->foundHandler = NULL;
	status = getByGetter(getter, fileno(dirFp));
	params->foundHandler = savePtr;
	params->forceDir = saveBool;

	if (status != CW_OK) {
		fclose(dirFp);
		if (status == params->foundSuppressErr) { status = getByGetterPath(getter, NULL, fd); }
		goto foundhandler;
	}

	rewind(dirFp);	
	if (params->forceDir || ((status = getFileByPath(dirFp, path, getter->fetchedNames, params, fd)) == CWG_IN_DIR_NO && (path[0] == 0 || strcmp(path, "/") == 0))) {
		rewind(dirFp);	
		int copyStatus;
		if ((copyStatus = copyStreamDataFildes(fd, dirFp)) != COPY_OK) {
			if (copyStatus == COPY_WRITE_ERR) { status = CWG_WRITE_ERR; }
			else { status = CW_SYS_ERR; }
		} else { status = CW_OK; }
	}	
	fclose(dirFp);	

	foundhandler:
	if (params->foundHandler != NULL) {
		if (status == params->foundSuppressErr) { status = CW_OK; }
		params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL;
	}
	
	return status;
}
