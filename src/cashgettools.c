#include "cashgettools.h"
#include "cashwebutils.h"

/* general constants */
#define LINE_BUF 150
#define MIME_STR_DEFAULT "application/octet-stream"

/* MongoDB constants */
#define MONGODB_STR_HEX_PREFIX "OP_RETURN "

/* BitDB HTTP constants */
#define BITDB_API_VER 3
#define BITDB_QUERY_BUF_SZ (80+strlen(BITDB_QUERY_DATA_TAG)+strlen(BITDB_QUERY_ID_TAG))
#define BITDB_ID_QUERY_BUF_SZ (20+CW_NAME_MAX_LEN)
#define BITDB_RESPHANDLE_QUERY_BUF_SZ (30+CW_NAME_MAX_LEN)
#define BITDB_HEADER_BUF_SZ 40
#define BITDB_QUERY_ID_TAG "n"
#define BITDB_QUERY_INFO_TAG "i"
#define BITDB_QUERY_TXID_TAG "t"
#define BITDB_QUERY_DATA_TAG "d"

/* Fetch typing */
typedef enum FetchType {
	BY_TXID,
	BY_INTXID,
	BY_NAMETAG
} FETCH_TYPE;

/*
 * struct for information to carry around during script execution
 */
struct CWG_script_pack {
	List *scriptStreams;
	const char *fromName;
	const char *revTxid;
	int atRev;
	int maxRev;
};

/*
 * initializes struct CWG_script_pack
 */
static inline void init_CWG_script_pack(struct CWG_script_pack *sp, List *scriptStreams, const char *fromName, const char *revTxid, int maxRev);

/*
 * copies struct CWG_script_pack from source to dest and increments current revision (atRev)
 */
static inline void copy_inc_CWG_script_pack(struct CWG_script_pack *dest, struct CWG_script_pack *source);

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
 * writes curl response to specified file stream
 */
static size_t writeResponseToStream(void *data, size_t size, size_t nmemb, FILE *respStream);

/*
 * fetches hex data (from BitDB HTTP endpoint) at specified ids and copies (in order) to specified location in memory 
 * id type is specified by FETCH_TYPE type
 * txids of fetched TXs can be written to txids, or can be set NULL; shouldn't be needed if type is BY_TXID
 */
static CW_STATUS fetchHexDataBitDBNode(const char **ids, size_t count, FETCH_TYPE type, const char *bitdbNode, bool bitdbRequestLimit, char **txids, char *hexDataAll);

/*
 * fetches hex data (from MongoDB populated by BitDB) at specified ids and copies (in order) to specified location in memory 
 * id type is specified by FETCH_TYPE type
 * txids of fetched TXs can be written to txids, or can be set NULL; shouldn't be needed if type is BY_TXID
 */
static CW_STATUS fetchHexDataMongoDB(const char **ids, size_t count, FETCH_TYPE type, mongoc_client_t *mongodbCli, char **txids, char *hexDataAll);

/*
 * wrapper for choosing between MongoDB query or BitDB HTTP request
 */
static inline CW_STATUS fetchHexData(const char **ids, size_t count, FETCH_TYPE type, struct CWG_params *params, char **txids, char *hexDataAll);
	
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
 * execute necessary action for given CW_OPCODE c
 * may involve pushing/popping stack, reading from scriptStreams (stored in given struct CWG_script_pack), and/or writing to fd
 */
static CW_STATUS execScriptCode(CW_OPCODE c, List *stack, struct CWG_script_pack *sp, struct CWG_params *params, int fd);

/*
 * executes cashweb script from scriptStream, writing anything specified by script to file descriptor fd
 */

static CW_STATUS execScript(struct CWG_script_pack *sp, struct CWG_params *params, int fd);

/*
 * fetches/traverses file at given nametag (according to script at nametag) and writes to specified file descriptor
 * revision can be specified for which version to get; -1 for latest
 * responsible for calling foundHandler if present in params; will be set to NULL upon call
 */
static CW_STATUS getFileByNametag(const char *name, int revision, struct CWG_params *params, int fd);

/*
 * fetches/traverses file at given txid and writes to specified file descriptor
 * responsible for calling foundHandler if present in params; will be set to NULL upon call
 */
static CW_STATUS getFileByTxid(const char *txid, struct CWG_params *params, int fd);

/*
 * initializes either MongoC or Curl depending on whether mongodb or bitdbNode is specified in params
 * should only be called from public functions that will get
 */
static CW_STATUS initFetcher(struct CWG_params *params);

/*
 * cleans up either MongoC or Curl depending on whether mongodb or bitdbNode is specified in params
 * should only be called from public functions that have called initFetcher()
 */
static void cleanupFetcher(struct CWG_params *params);

/* ------------------------------------- PUBLIC ------------------------------------- */

CW_STATUS CWG_get_by_txid(const char *txid, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; } 

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	status = getFileByTxid(txid, params, fd);
	params->foundHandler = savePtr;
	
	cleanupFetcher(params);
	return status;
}

CW_STATUS CWG_get_by_nametag(const char *name, int revision, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = initFetcher(params)) != CW_OK) { return status; } 

	void (*savePtr) (CW_STATUS, void *, int) = params->foundHandler;
	status = getFileByNametag(name, revision, params, fd);
	params->foundHandler = savePtr;
	
	cleanupFetcher(params);
	return status;
}

CW_STATUS CWG_dir_path_to_identifier(FILE *dirFp, const char *dirPath, char *pathTxid) {
	CW_STATUS status = CWG_IN_DIR_NO;

	char pathTxidBytes[CW_TXID_BYTES];
	memset(pathTxidBytes, 0, CW_TXID_BYTES);

	struct DynamicMemory line;
	initDynamicMemory(&line);

	bool found = false;
	int count = 0;
	int readlineStatus;
	while ((readlineStatus = safeReadLine(&line, LINE_BUF, dirFp)) == READLINE_OK) {
		if (strlen(line.data) < 1) { break; }
				
		if (!found) {
			++count;
			if (strcmp(line.data, dirPath) == 0) {
				found = true;
				status = CW_OK;
			}
		}
	}
	if (ferror(dirFp)) { perror("fgets() failed on directory index"); status = CW_SYS_ERR; }
	else if (readlineStatus == READLINE_ERR) { status = CW_SYS_ERR; }
	if (status != CW_OK) { goto cleanup; }

	if (count > 0) {
		if (fseek(dirFp, CW_TXID_BYTES*(count-1), SEEK_CUR) < 0) {
			perror("fseek() SEEK_CUR failed on directory index");
			status = CWG_FILE_ERR;
			goto cleanup;
		}
		if (fread(pathTxidBytes, CW_TXID_BYTES, 1, dirFp) < 1) {
			if (ferror(dirFp)) {
				perror("fread() failed on directory index");
				status = CW_SYS_ERR;
			} else { status = CWG_FILE_ERR; }
			goto cleanup;
		}
		byteArrToHexStr(pathTxidBytes, CW_TXID_BYTES, pathTxid);
	} else { status = CWG_IN_DIR_NO; } // this probably isn't necessary to set, as status starts at this value, but just in case

	cleanup:
		freeDynamicMemory(&line);
		return status;
}

const char *CWG_errno_to_msg(int errNo) {
	switch (errNo) {
		case CW_DATADIR_NO:
			return "Unable to find proper cashwebtools data directory";
		case CW_CALL_NO:
			return "Bad call to cashgettools function; may be bad implementation";
		case CW_SYS_ERR:
			return "There was an unexpected system error. This may be problem with cashgettools";	
		case CWG_IN_DIR_NO:
			return "Requested file doesn't exist in specified directory";
		case CWG_IS_DIR_NO:
			return "Requested file isn't a directory";
		case CWG_FETCH_NO:
			return "Requested file doesn't exist, check identifier";
		case CWG_METADATA_NO:
			return "Requested file's metadata is invalid or nonexistent, check identifier";	
		case CWG_FETCH_ERR:
			return "There was an unexpected error in querying the blockchain";
		case CWG_WRITE_ERR:
			return "There was an unexpected error in writing the file";
		case CWG_FILE_LEN_ERR:
		case CWG_FILE_DEPTH_ERR:
		case CWG_FILE_ERR:
			return "There was an unexpected error in interpreting the file. The file may be encoded incorrectly (i.e. inaccurate metadata/structuring), or there is a problem with cashgettools";
		case CWG_SCRIPT_ERR:
			return "Requested nametag's encoded script is either invalid or lacks a file reference";
		case CWG_SCRIPT_CODE_NO:
		case CWG_SCRIPT_NO:
		default:
			return "Unexpected error code. This is likely an issue with cashgettools";
	}
}

/* ---------------------------------------------------------------------------------- */

static inline void init_CWG_script_pack(struct CWG_script_pack *sp, List *scriptStreams, const char *fromName, const char *revTxid, int maxRev) {
	sp->scriptStreams = scriptStreams;
	sp->fromName = fromName;
	sp->revTxid = revTxid;
	sp->atRev = 0;
	sp->maxRev = maxRev;
}

static inline void copy_inc_CWG_script_pack(struct CWG_script_pack *dest, struct CWG_script_pack *source) {
	dest->scriptStreams = source->scriptStreams;
	dest->fromName = source->fromName;
	dest->revTxid = source->revTxid;
	dest->atRev = source->atRev+1;
	dest->maxRev = source->maxRev;
}

static void protocolCheck(uint16_t pVer) {
	if (pVer > CW_P_VER) {
		fprintf(stderr, "WARNING: requested file signals a newer cashweb protocol version than this client uses (client: CWP %u, file: CWP %u).\nWill attempt to read anyway, in case this is inaccurate or the protocol upgrade is trivial.\nIf there is a new protocol version available, it is recommended you upgrade.\n", CW_P_VER, pVer);
	}
}

static CW_STATUS cwTypeToMimeStr(CW_TYPE cwType, struct CWG_params *cgp) {
	if (!cgp->saveMimeStr) { return CW_OK; }
	(*cgp->saveMimeStr)[0] = 0;
	if (cwType <= CW_T_MIMESET) { strcat(*cgp->saveMimeStr, MIME_STR_DEFAULT); return CW_OK; }
	if (cgp->datadir == NULL) { cgp->datadir = CW_INSTALL_DATADIR_PATH; }

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
		fprintf(stderr, "fopen() failed on path %s; unable to open cashweb mime.types\n", mtFilePath);
		perror(NULL);
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
			fprintf(stderr, "unable to parse for mimetype string, mime.types may be invalid; defaults to cashgettools MIME_STR_DEFAULT\n");
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
			fprintf(stderr, "invalid cashweb type (numeric %u); defaults to MIME_STR_DEFAULT\n", cwType);
		}
		strcat(*cgp->saveMimeStr, MIME_STR_DEFAULT);
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
	if ((n = write(fd, fileByteData, bytesToWrite)) < bytesToWrite) {
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

static size_t writeResponseToStream(void *data, size_t size, size_t nmemb, FILE *respStream) {
	return fwrite(data, size, nmemb, respStream)*size;
}

static CW_STATUS fetchHexDataBitDBNode(const char **ids, size_t count, FETCH_TYPE type, const char *bitdbNode, bool bitdbRequestLimit, char **txids, char *hexDataAll) {
	if (count < 1) { return CWG_FETCH_NO; }
	if (type == BY_NAMETAG && count != 1) {
		fprintf(stderr, "current implementation doesn't allow for fetchHexData() querying more than one nametag; problem with cashgettools\n");
		return CW_SYS_ERR;
	}

	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() failed\n"); return CWG_FETCH_ERR; }

	int printed = 0;
	// construct query
	char *idQuery = malloc(BITDB_ID_QUERY_BUF_SZ*count); idQuery[0] = 0;
	for (int i=0; i<count; i++) {
		switch (type) {
			case BY_TXID:
				printed = snprintf(idQuery+strlen(idQuery), BITDB_ID_QUERY_BUF_SZ, "{\"tx.h\":\"%s\"},", ids[i]);
				break;
			case BY_INTXID:
				printed = snprintf(idQuery+strlen(idQuery), BITDB_ID_QUERY_BUF_SZ, "{\"in.e.h\":\"%s\"},", ids[i]);
				break;
			case BY_NAMETAG:
				if (strlen(ids[i])-strlen(CW_NAMETAG_PREFIX) > CW_NAME_MAX_LEN) {
					fprintf(stderr, "cashgettools: nametag queried is too long\n");
					free(idQuery);
					curl_easy_cleanup(curl);
					return CW_CALL_NO;
				}
				printed = snprintf(idQuery+strlen(idQuery), BITDB_ID_QUERY_BUF_SZ, "{\"out.s2\":\"%s\"},", ids[i]);
				break;
			default:
				fprintf(stderr, "invalid FETCH_TYPE; problem with cashgettools\n");
				free(idQuery);
				curl_easy_cleanup(curl);
				return CW_SYS_ERR;
		}
		if (printed >= BITDB_ID_QUERY_BUF_SZ) {
			fprintf(stderr, "BITDB_ID_QUERY_BUF_SZ set too small; problem with cashgettools\n");
			free(idQuery);
			curl_easy_cleanup(curl);
			return CW_SYS_ERR;
		}
	}
	idQuery[strlen(idQuery)-1] = 0;

	// construct response handler for query
	char respHandler[BITDB_RESPHANDLE_QUERY_BUF_SZ];
	switch (type) {
		case BY_TXID:
			printed = snprintf(respHandler, sizeof(respHandler), "{%s:.out[0].h1,%s:.tx.h}", BITDB_QUERY_DATA_TAG, BITDB_QUERY_ID_TAG);
			break;
		case BY_INTXID:
			printed = snprintf(respHandler, sizeof(respHandler), "{%s:.out[0].h1,%s:.in[0].e.h,%s:.in[0].e.i,%s:.tx.h}", BITDB_QUERY_DATA_TAG, BITDB_QUERY_ID_TAG, BITDB_QUERY_INFO_TAG, BITDB_QUERY_TXID_TAG);
			break;
		case BY_NAMETAG:
			printed = snprintf(respHandler, sizeof(respHandler), "{%s:.out[0].h1,%s:.out[0].s2,%s:.tx.h}", BITDB_QUERY_DATA_TAG, BITDB_QUERY_ID_TAG, BITDB_QUERY_TXID_TAG);
			break;
		default:
			fprintf(stderr, "invalid FETCH_TYPE; problem with cashgettools\n");
			free(idQuery);
			curl_easy_cleanup(curl);
			return CW_SYS_ERR;
	}
	if (printed >= sizeof(respHandler)) {
		fprintf(stderr, "BITDB_RESPHANDLE_QUERY_BUF_SZ set too small; problem with cashgettools\n");
		free(idQuery);
		curl_easy_cleanup(curl);
		return CW_SYS_ERR;
	}

	char *specifiersStr = type == BY_NAMETAG ? ",\"sort\":{\"blk.i\":1,\"tx.h\":1},\"limit\":1" : "";
	char query[BITDB_QUERY_BUF_SZ + strlen(specifiersStr) + strlen(idQuery) + strlen(respHandler) + 1];
	printed = snprintf(query, sizeof(query), 
		  "{\"v\":%d,\"q\":{\"find\":{\"$or\":[%s]}%s},\"r\":{\"f\":\"[.[]|%s]\"}}",
	    	  BITDB_API_VER, idQuery, specifiersStr, respHandler);
	free(idQuery);
	if (printed >= sizeof(query)) {
		fprintf(stderr, "BITDB_QUERY_BUF_SZ set too small; problem with cashgettools\n");
		curl_easy_cleanup(curl);
		return CW_SYS_ERR;
	}

	char *queryB64;
	if ((queryB64 = b64_encode((const unsigned char *)query, strlen(query))) == NULL) { perror("b64 encode failed");
											    curl_easy_cleanup(curl); return CW_SYS_ERR; }
	char url[strlen(bitdbNode) + strlen(queryB64) + 1 + 1];

	// construct url from query
	strcpy(url, bitdbNode);
	strcat(url, "/");
	strcat(url, queryB64);
	free(queryB64);	

	// initializing variable-length arrays before goto statements
	const char *hexDataPtrs[count];
	bool added[count];

	// send curl request
	FILE *respFp = tmpfile();
	if (respFp == NULL) { perror("tmpfile() failed"); curl_easy_cleanup(curl); return CW_SYS_ERR; }
	struct curl_slist *headers = NULL;
	if (bitdbRequestLimit) { // this bit is to trick a server's request limit, although won't necessarily work with every server
		char buf[BITDB_HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d",
			rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeResponseToStream);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, respFp);
	res = curl_easy_perform(curl);
	if (headers) { curl_slist_free_all(headers); }
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		fclose(respFp);
		return CWG_FETCH_ERR;
	} 
	rewind(respFp);

	CW_STATUS status = CW_OK;	
	
	// load response json from file and handle potential errors
	json_error_t jsonError;
	json_t *respJson = json_loadf(respFp, 0, &jsonError);
	if (respJson == NULL) {
		long respSz = fileSize(fileno(respFp));
		char respMsg[respSz+1];
		respMsg[fread(respMsg, 1, respSz, respFp)] = 0;
		if ((strlen(respMsg) < 1 && count > 1) || (strstr(respMsg, "URI") && strstr(respMsg, "414"))) { // catch for Request-URI Too Large or empty response body
			int firstCount = count/2;
			CW_STATUS status1 = fetchHexDataBitDBNode(ids, firstCount, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll);
			CW_STATUS status2 = fetchHexDataBitDBNode(ids+firstCount, count-firstCount, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll+strlen(hexDataAll));
			status = status1 > status2 ? status1 : status2;
			goto cleanup;
		}
		else if (strstr(respMsg, "html")) {
			fprintf(stderr, "HTML response error unhandled in cashgettools:\n%s\n", respMsg);
			status = CWG_FETCH_ERR;
			goto cleanup;
		}
		else {
			fprintf(stderr, "jansson error in parsing response from BitDB node: %s\nResponse:\n%s\n", jsonError.text, respMsg);
			status = CW_SYS_ERR;
			goto cleanup;
		}
	}

	// parse for hex datas at matching ids within both unconfirmed and confirmed transaction json arrays
	json_t *jsonArrs[2] = { json_object_get(respJson, "c"), json_object_get(respJson, "u") };
	size_t index;
	json_t *dataJson;
	char *jsonDump;
	const char *dataId;
	const char *dataHex;
	const char *dataTxid;
	int dataVout = 0;
	bool matched;
	memset(added, 0, count);
	for (int i=0; i<count; i++) {
		matched = false;
		for (int a=0; a<sizeof(jsonArrs)/sizeof(jsonArrs[0]); a++) {
			if (!jsonArrs[a]) {
				jsonDump = json_dumps(respJson, 0);
				fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", jsonDump);
				free(jsonDump);
				status = CWG_FETCH_ERR;
				goto cleanup;
			}
			
			json_array_foreach(jsonArrs[a], index, dataJson) {
				if ((dataId = json_string_value(json_object_get(dataJson, BITDB_QUERY_ID_TAG))) == NULL ||
				    (dataHex = json_string_value(json_object_get(dataJson, BITDB_QUERY_DATA_TAG))) == NULL) {
				    	jsonDump = json_dumps(jsonArrs[a], 0);
					fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", jsonDump);
					free(jsonDump);
					status = CWG_FETCH_ERR; goto cleanup;
				}
				if (type == BY_INTXID) { dataVout = json_integer_value(json_object_get(dataJson, BITDB_QUERY_INFO_TAG)); }

				if (!added[i] && strcmp(ids[i], dataId) == 0 && (type != BY_INTXID || dataVout == CW_REVISION_INPUT_VOUT)) {
					hexDataPtrs[i] = dataHex;	
					if (txids) {
						if (type == BY_TXID) { dataTxid = dataId; }
						else if ((dataTxid = json_string_value(json_object_get(dataJson, BITDB_QUERY_TXID_TAG))) == NULL) {
							jsonDump = json_dumps(jsonArrs[a], 0);
							fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", jsonDump);
							free(jsonDump);
							status = CWG_FETCH_ERR; goto cleanup;		
						}
						txids[i][0] = 0; strncat(txids[i], dataTxid, CW_TXID_CHARS);
					}
					added[i] = true;
					matched = true;
					break;
				}
			}
			if (matched) { break; } 
		}
		if (!matched) { status = CWG_FETCH_NO; goto cleanup; }
	}
	
	hexDataAll[0] = 0;
	for (int i=0; i<count; i++) { strncat(hexDataAll, hexDataPtrs[i], CW_TX_DATA_CHARS); }

	cleanup:
		json_decref(respJson);	
		fclose(respFp);
		return status;
}

static CW_STATUS fetchHexDataMongoDB(const char **ids, size_t count, FETCH_TYPE type, mongoc_client_t *mongodbCli, char **txids, char *hexDataAll) {
	if (count < 1) { return CWG_FETCH_NO; }
	if (type == BY_NAMETAG && count != 1) {
		fprintf(stderr, "current implementation doesn't allow for fetchHexData() querying more than one nametag; problem with cashgettools\n");
		return CW_SYS_ERR;
	}

	CW_STATUS status = CW_OK;
	
	hexDataAll[0] = 0;
	mongoc_collection_t *colls[2] = { mongoc_client_get_collection(mongodbCli, "bitdb", "confirmed"), 
					  mongoc_client_get_collection(mongodbCli, "bitdb", "unconfirmed") };
	bson_t *query = NULL;
	bson_t *opts = NULL;
	switch (type) {
		case BY_TXID:
			opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "_id", BCON_BOOL(false), "}");
			break;
		case BY_INTXID:
			opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "in", BCON_BOOL(true), "tx", BCON_BOOL(true), "_id", BCON_BOOL(false), "}");
			break;
		case BY_NAMETAG:
			opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "tx", BCON_BOOL(true), "_id", BCON_BOOL(false), "}",
					"sort", "{", "blk.i", BCON_INT32(1), "tx.h", BCON_INT32(1), "}",
					"limit", BCON_INT64(1));
			break;
		default:
			fprintf(stderr, "invalid FETCH_TYPE; problem with cashgettools\n");
			goto cleanup;
	}	

	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *res;
	char *resStr;
	json_t *resJson;
	char  *jsonDump;
	json_error_t jsonError;
	char hexData[CW_TX_DATA_CHARS+1];
	char *token;
	const char *str;
	const char *txid;
	int vout;
	size_t hexPrefixLen = strlen(MONGODB_STR_HEX_PREFIX);
	bool matched;
	for (int i=0; i<count; i++) { 
		matched = false;
		switch (type) {
			case BY_TXID:
				query = BCON_NEW("tx.h", ids[i]);
				break;
			case BY_INTXID:
				query = BCON_NEW("in.e.h", ids[i]);
				break;
			case BY_NAMETAG:
				query = BCON_NEW("out.s2", ids[i]);
				break;
		}
		for (int c=0; c<sizeof(colls)/sizeof(colls[0]); c++) {
			cursor = mongoc_collection_find_with_opts(colls[c], query, opts, NULL);
			while (mongoc_cursor_next(cursor, &res)) { 
				resStr = bson_as_relaxed_extended_json(res, NULL);
				resJson = json_loads(resStr, JSON_ALLOW_NUL, &jsonError);
				bson_free(resStr);
				if (resJson == NULL) {
					fprintf(stderr, "jansson error in parsing result from MongoDB query: %s\nResponse:\n%s\n", jsonError.text, resStr);
					status = CW_SYS_ERR;
					break;
				}

				if (type == BY_INTXID) {
					// gets json array at key 'out' -> object at array index 0 -> object at key 'e' -> object at key 'i' (.in[0].e.i)
					vout = json_integer_value(json_object_get(json_object_get(json_array_get(json_object_get(resJson, "in"), 0), "e"), "i"));
					if (vout != CW_REVISION_INPUT_VOUT) { continue; }
				}

				// gets json array at key 'out' -> object at array index 0 -> object at key 'str' (.out[0].str)
				str = json_string_value(json_object_get(json_array_get(json_object_get(resJson, "out"), 0), "str"));	
				if (!str) {
					jsonDump = json_dumps(resJson, 0);
					fprintf(stderr, "invalid response from MongoDB:\n%s\n", jsonDump);
					free(jsonDump);
					status = CWG_FETCH_ERR;
					break;
				} 
				if (strncmp(str, MONGODB_STR_HEX_PREFIX, hexPrefixLen) != 0) { status = CWG_FILE_ERR; break; }

				hexData[0] = 0; strncat(hexData, str+hexPrefixLen, CW_TX_DATA_CHARS);
				if ((token = strchr(hexData, ' '))) { *token = 0; }

				strncat(hexDataAll, hexData, CW_TX_DATA_CHARS);
				if (txids) {
					if (type == BY_TXID) { txid = ids[i]; }
					else { txid = json_string_value(json_object_get(json_object_get(resJson, "tx"), "h")); }	
					if (!txid) {
						jsonDump = json_dumps(resJson, 0);
						fprintf(stderr, "invalid response from MongoDB:\n%s\n", jsonDump);
						free(jsonDump);
						status = CWG_FETCH_ERR;
						break;
					}
					txids[i][0] = 0; strncat(txids[i], txid, CW_TXID_CHARS);
				}
				matched = true;
				break;
			}
			if (mongoc_cursor_error(cursor, &error)) {
				fprintf(stderr, "ERROR: MongoDB query failed\nMessage: %s\n", error.message);
				status = CWG_FETCH_ERR;
			} 
			mongoc_cursor_destroy(cursor);
			if (status != CW_OK) { break; }
		}
		bson_destroy(query);
		if (!matched) { status = status == CW_OK ? CWG_FETCH_NO : status; break; }
	}

	cleanup:
		if (opts) { bson_destroy(opts); }
		for (int c=0; c<sizeof(colls)/sizeof(colls[0]); c++) { mongoc_collection_destroy(colls[c]); }
		return status;
}

static inline CW_STATUS fetchHexData(const char **ids, size_t count, FETCH_TYPE type, struct CWG_params *params, char **txids, char *hexDataAll) {
	if (params->mongodbCli) { return fetchHexDataMongoDB(ids, count, type,  params->mongodbCli, txids, hexDataAll); }
	else if (params->bitdbNode) { return fetchHexDataBitDBNode(ids, count, type, params->bitdbNode, params->bitdbRequestLimit, txids, hexDataAll); }
	else {
		fprintf(stderr, "ERROR: neither MongoDB nor BitDB HTTP endpoint address is set in cashgettools implementation\n");
		return CW_CALL_NO;
	}
}

static CW_STATUS traverseFileTree(const char *treeHexData, List *partialTxids[], int suffixLen, int depth,
			    	  struct CWG_params *params, struct CW_file_metadata *md, int fd) {

	char *partialTxid;
	int partialTxidFill = partialTxids != NULL && (partialTxid = popFront(partialTxids[0])) != NULL ?
			      CW_TXID_CHARS-strlen(partialTxid) : 0;	
	
	int numChars = strlen(treeHexData+partialTxidFill)-suffixLen;
	int txidsCount = numChars/CW_TXID_CHARS + (partialTxidFill ? 1 : 0);
	if (txidsCount < 1) { return CW_OK; }

	CW_STATUS status = CW_OK;

	char *txids[txidsCount];
	for (int i=0; i<txidsCount; i++) {
		if ((txids[i] = malloc(CW_TXID_CHARS+1)) == NULL) { perror("malloc failed"); status  = CW_SYS_ERR; }
	}
	if (status != CW_OK) { goto cleanup; }

	if (partialTxidFill) {
		strcpy(txids[0], partialTxid);
		strncat(txids[0], treeHexData, partialTxidFill);
		free(partialTxid);
	}
	const char *txidPtr = treeHexData+partialTxidFill;
	int sTxidsCount = (partialTxidFill ? 1 : 0);
	if (txidsCount < sTxidsCount) { status = CWG_FILE_ERR; goto cleanup; }
	for (int i=sTxidsCount; i<txidsCount; i++) {
		strncpy(txids[i], txidPtr, CW_TXID_CHARS);
		txids[i][CW_TXID_CHARS] = 0;
		txidPtr += CW_TXID_CHARS;
	}
	char partialTemp[CW_TXID_CHARS+1]; partialTemp[0] = 0;
	strncat(partialTemp, txidPtr, numChars-(txidPtr-(treeHexData+partialTxidFill)));

	// frees the tree hex data if in recursive call
	if (depth > 0) { free((char *)treeHexData); }

	char *hexDataAll = malloc(CW_TX_DATA_CHARS*txidsCount + 1);
	if (hexDataAll == NULL) { perror("malloc failed"); status =  CW_SYS_ERR; goto cleanup; }
	if ((status = fetchHexData((const char **)txids, txidsCount, BY_TXID, params, NULL, hexDataAll)) == CW_OK) { 
		if (partialTxids != NULL) {
			char *partialTxidN = malloc(CW_TXID_CHARS+1);
			if (partialTxidN == NULL) { perror("malloc failed"); free(hexDataAll); status = CW_SYS_ERR; goto cleanup; }
			strcpy(partialTxidN, partialTemp);
			if (!addFront(partialTxids[1], partialTxidN)) {
				perror("mylist addFront() failed");
				free(hexDataAll);
				status = CW_SYS_ERR;
				goto cleanup;
			}
		}

		if (depth+1 < md->depth) {
			status = traverseFileTree(hexDataAll, partialTxids, 0, depth+1, params, md, fd);
		} else {
			if ((status = writeHexDataStr(hexDataAll, 0, fd)) != CW_OK) { goto cleanup; }
			free(hexDataAll);

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
		strcpy(hexData, hexDataNext);
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
		fprintf(stderr, "cashgettools: nametag specified for get is too long (maximum %lu characters)\n", CW_NAME_MAX_LEN);
		return CW_CALL_NO;
	}

	CW_STATUS status;

	char hexDataStart[CW_TX_DATA_CHARS+1];
	struct CW_file_metadata md;

	char nametag[strlen(CW_NAMETAG_PREFIX) + strlen(name) + 1]; nametag[0] = 0;
	strcat(nametag, CW_NAMETAG_PREFIX);
	strcat(nametag, name);
	char *nametagPtr = nametag;
	if ((status = fetchHexData((const char **)&nametagPtr, 1, BY_NAMETAG, params, txidPtr, hexDataStart)) != CW_OK) { return status; }
	if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { return status; }
	protocolCheck(md.pVer);

	return traverseFile(hexDataStart, params, &md, fileno(stream));
}

static CW_STATUS execScriptCode(CW_OPCODE c, List *stack, struct CWG_script_pack *sp, struct CWG_params *params, int fd) {
	switch (c) {
		case CW_OP_TERM:
			return CWG_SCRIPT_NO;
		case CW_OP_NEXTREV:
		{
			if (sp->maxRev >= 0 && sp->atRev >= sp->maxRev) { return CWG_SCRIPT_CODE_NO; }

			char nextRevTxid[CW_TXID_CHARS+1]; char *nextRevTxidPtr = nextRevTxid;
			FILE *nextScriptStream = tmpfile();
			if (nextScriptStream == NULL) { perror("tmpfile() failed"); return CW_SYS_ERR; }

			CW_STATUS status;
			if ((status = getScriptByInTxid(sp->revTxid, params, &nextRevTxidPtr, nextScriptStream)) != CW_OK) {
				fclose(nextScriptStream);
				if (status == CWG_FETCH_NO) { return CWG_SCRIPT_CODE_NO; }
				return status;
			}
			rewind(nextScriptStream);

			if (!addFront(sp->scriptStreams, nextScriptStream)) { perror("mylist addFront() failed"); fclose(nextScriptStream); return CW_SYS_ERR; }
			struct CWG_script_pack spN;
			copy_inc_CWG_script_pack(&spN, sp);
			spN.revTxid = nextRevTxid;

			status = execScript(&spN, params, fd);

			fclose(nextScriptStream);
			return status;
		}
		case CW_OP_PUSHTXID:
		{
			FILE *scriptStream = peekFront(sp->scriptStreams);
			if (!scriptStream) { fprintf(stderr, "mylist peekFront() failed on scriptStreams; problem with cashgettools"); return CW_SYS_ERR; }

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

			CW_STATUS status = getFileByTxid(txid, params, fd);

			free(txid);
			if (status == CWG_FETCH_NO) { return CWG_SCRIPT_ERR; }
			return status;
		}
		case CW_OP_WRITEFROMNAMETAG:
		{
			char *name;
			if ((name = popFront(stack)) == NULL) { return CWG_SCRIPT_ERR; }
			else if (!CW_is_valid_name(name)) { free(name); return CWG_SCRIPT_ERR; }

			CW_STATUS status = getFileByNametag(name, CWG_REV_LATEST, params, fd);

			free(name);
			if (status == CWG_FETCH_NO || status == CW_CALL_NO) { return CWG_SCRIPT_ERR; }
			return status;
		}
		case CW_OP_WRITEFROMPREV:
		{
			if (sp->atRev < 1) { return CWG_SCRIPT_ERR; }
			FILE *startScriptStream = peekLast(sp->scriptStreams);
			if (!startScriptStream) { fprintf(stderr, "mylist peekLast() failed on scriptStreams; problem with cashgettools"); return CW_SYS_ERR; }

			long savePos = ftell(startScriptStream);
			if (savePos < 0) { perror("ftell() failed on startScriptStream"); return CW_SYS_ERR; }

			FILE *scriptStreamDup = tmpfile();
			if (!scriptStreamDup) { perror("tmpfile() failed"); return CW_SYS_ERR; }

			rewind(startScriptStream);
			if (!copyStreamData(scriptStreamDup, startScriptStream)) { fclose(scriptStreamDup); return CW_SYS_ERR; }
			if (fseek(startScriptStream, savePos, SEEK_SET) < 0) { perror("fseek() failed on startScriptStream"); fclose(scriptStreamDup); return CW_SYS_ERR; }

			List scriptStreams;
			initList(&scriptStreams);
			if (!addFront(&scriptStreams, scriptStreamDup)) { perror("mylist addFront() failed"); fclose(scriptStreamDup); return CW_SYS_ERR; }

			struct CWG_script_pack spD;
			init_CWG_script_pack(&spD, &scriptStreams, sp->fromName, sp->revTxid, sp->atRev-1);

			CW_STATUS status = execScript(&spD, params, fd);

			removeAllNodes(&scriptStreams, false);	
			fclose(scriptStreamDup);
			return status;
		}
		case CW_OP_PUSHUCHAR:
		case CW_OP_PUSHUSHORT:
		case CW_OP_PUSHUINT:
		{
			FILE *scriptStream = peekFront(sp->scriptStreams);
			if (!scriptStream) { fprintf(stderr, "mylist peekFront() failed on scriptStreams; problem with cashgettools"); return CW_SYS_ERR; }

			int numBytes;
			switch (c) {
				case CW_OP_PUSHUCHAR:
					numBytes = sizeof(uint8_t);
					break;
				case CW_OP_PUSHUSHORT:
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
			char tmpname[] = "CWtmpstore-XXXXXX";
			int tfd = mkstemp(tmpname);
			if (tfd < 0) { perror("mkstemp() failed"); return CW_SYS_ERR; }
			unlink(tmpname);

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
			if ((status = execScriptCode(writeOp, stack, sp, params, tfd)) != CW_OK) { close(tfd); return status; }

			uint32_t sfd = (uint32_t)tfd;
			char sfdHexStr[(sizeof(sfd)*2)+1];
			if (!intToNetHexStr(&sfd, sizeof(sfd), sfdHexStr)) { status = CW_SYS_ERR; }
			else if (!addFront(stack, sfdHexStr)) { perror("mylist addFront() failed"); status = CW_SYS_ERR; }
			if (status != CW_OK) { close(tfd); }

			return status;
		}
		case CW_OP_WRITESEGFROMSTORED:
		case CW_OP_WRITERANGEFROMSTORED:
		{
			CW_STATUS status = CW_OK;
			uint32_t low;
			uint32_t high;
			uint32_t sfd;

			char *lowHexStr = popFront(stack);
			if (!lowHexStr) { return CWG_SCRIPT_ERR; }
			size_t lowBytes = strlen(lowHexStr)/2;

			if (lowBytes == sizeof(uint8_t)) {  low = (uint32_t)strtoul(lowHexStr, NULL, 16); }
			else if (lowBytes == sizeof(uint16_t) || lowBytes == sizeof(uint32_t)) {
				if (!netHexStrToInt(lowHexStr, lowBytes, &low)) { status = CW_SYS_ERR; }
			}
			else { status = CWG_SCRIPT_ERR; }
			free(lowHexStr);
			if (status != CW_OK) { return status; }

			char *highHexStr = popFront(stack);
			if (!highHexStr) { return CWG_SCRIPT_ERR; }
			size_t highBytes = strlen(highHexStr)/2;

			if (highBytes == sizeof(uint8_t)) {  high = (uint32_t)strtoul(highHexStr, NULL, 16); }
			else if (highBytes == sizeof(uint16_t) || highBytes == sizeof(uint32_t)) {
				if (!netHexStrToInt(highHexStr, highBytes, &high)) { status = CW_SYS_ERR; }
			}
			else { status = CWG_SCRIPT_ERR; }		
			free(highHexStr);
			if (status != CW_OK) { return status; }

			char *sfdHexStr = peekFront(stack);
			if (!sfdHexStr) { return CWG_SCRIPT_ERR; }
			size_t sfdBytes = strlen(sfdHexStr)/2;

			if (sfdBytes == sizeof(uint32_t)) {
				if (!netHexStrToInt(sfdHexStr, sfdBytes, &sfd)) { status = CW_SYS_ERR; }
			}
			else { status = CWG_SCRIPT_ERR; }
			free(sfdHexStr);
			if (status != CW_OK) { return status; }

			int tfd = (int)sfd;
			if (fcntl(tfd, F_GETFD) == -1) { return CWG_SCRIPT_ERR; }
			if (lseek(tfd, 0, SEEK_SET) < 0) { return CWG_SCRIPT_ERR; } // in this case, presumed to not be file descriptor from mkstemp()

			if (lseek(tfd, (off_t)low, SEEK_SET) < 0) { close(tfd); return CWG_SCRIPT_ERR; }
			size_t toWrite;
			switch (c) {
				case CW_OP_WRITESEGFROMSTORED:
					if (high < 1) { close(tfd); return CWG_SCRIPT_ERR; }
					toWrite = (size_t)high;
					break;
				default:
					if (high <= low) { close(tfd); return CWG_SCRIPT_ERR; }
					toWrite = (size_t)(high - low);
					break;
			}
			if (toWrite == 0) { close(tfd); return CWG_SCRIPT_ERR; }

			char buf[FILE_DATA_BUF];
			size_t maxWriteChunk = toWrite < sizeof(buf) ? toWrite : sizeof(buf);
			ssize_t r;
			ssize_t w;
			while (toWrite > 0 && (r = read(tfd, buf, toWrite < maxWriteChunk ? toWrite : maxWriteChunk)) > 0) {
				if ((w = write(fd, buf, r)) < r) {
					if (w < 0) { perror("write() failed"); }
					close(tfd);
					return CWG_WRITE_ERR;
				}
				toWrite -= w;
			}
			if (r < 0) { perror("read() failed"); close(tfd); return CW_SYS_ERR; }

			return CW_OK;
		}
		case CW_OP_DROPSTORED:
		{
			CW_STATUS status = CW_OK;

			char *sfdHexStr = popFront(stack);
			if (!sfdHexStr) { return CWG_SCRIPT_ERR; }
			size_t sfdBytes = strlen(sfdHexStr)/2;

			uint32_t sfd;
			if (sfdBytes == sizeof(uint32_t)) {
				if (!netHexStrToInt(sfdHexStr, sfdBytes, &sfd)) { status = CW_SYS_ERR; }
			}
			else { status = CWG_SCRIPT_ERR; }
			free(sfdHexStr);
			if (status != CW_OK) { return status; }

			int tfd = (int)sfd;
			if (fcntl(tfd, F_GETFD) == -1) { return CWG_SCRIPT_ERR; }
			if (lseek(tfd, 0, SEEK_SET) < 0) { return CWG_SCRIPT_ERR; } // in this case, presumed to not be file descriptor from mkstemp()

			close(tfd);
			return CW_OK;
		}
		case CW_OP_PATHREPLACE:
		{
			char *toReplace;
			char *replacement;
			if ((toReplace = popFront(stack)) == NULL) { return CWG_SCRIPT_ERR; }
			if ((replacement = popFront(stack)) == NULL) { free(toReplace); return CWG_SCRIPT_ERR; }

			if (params->dirPath && params->dirPathReplace && strcmp(params->dirPath, toReplace) == 0) {
				if (params->dirPathReplaceToFree) { free(params->dirPathReplace); }	
				params->dirPathReplace = replacement;
				params->dirPathReplaceToFree = true;
			} else { free(replacement); }
			free(toReplace);

			return CW_OK;
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
			else { len = (size_t)c; }

			FILE *scriptStream = peekFront(sp->scriptStreams);
			if (!scriptStream) { fprintf(stderr, "mylist peekFront() failed on scriptStreams; problem with cashgettools"); return CW_SYS_ERR; }

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
	FILE *scriptStream = peekFront(sp->scriptStreams);
	if (!scriptStream) { fprintf(stderr, "mylist peekFront() failed on scriptStreams; problem with cashgettools"); return CW_SYS_ERR; }

	CW_STATUS status = CW_OK;

	List stack;
	initList(&stack);	

	int c;	
	CW_OPCODE code;
	while ((c = getc(scriptStream)) != EOF) {
		code = (CW_OPCODE)c;
		// if script is invalid, will attempt to replace with next revision; if it isn't there, will return CWG_SCRIPT_ERR
		if ((status = execScriptCode(code, &stack, sp, params, fd)) == CWG_SCRIPT_ERR) {
			removeAllNodes(&stack, true);
			if ((status = execScriptCode(CW_OP_NEXTREV, &stack, sp, params, fd)) == CWG_SCRIPT_CODE_NO) {
				status = CWG_SCRIPT_ERR;
			}
			goto cleanup;
		}
		else if (status == CWG_SCRIPT_CODE_NO) { status = CW_OK; continue; }
		else if (status != CW_OK) { goto cleanup; }
	}
	if (ferror(scriptStream)) { perror("getc() failed on scriptStream"); status = CW_SYS_ERR; goto cleanup; }

	cleanup:
		removeAllNodes(&stack, true);
		popFront(sp->scriptStreams);
		return status;
}

static CW_STATUS getFileByNametag(const char *name, int revision, struct CWG_params *params, int fd) {	
	CW_STATUS status;

	char revTxid[CW_TXID_CHARS+1]; char *revTxidPtr = revTxid;
	FILE *scriptStream = NULL;		

	if ((scriptStream = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto foundhandler; }	
	if ((status = getScriptByNametag(name, params, &revTxidPtr, scriptStream)) != CW_OK) { goto foundhandler; }
	rewind(scriptStream);

	List scriptStreams;
	initList(&scriptStreams);
	if (!addFront(&scriptStreams, scriptStream)) { perror("mylist addFront() failed"); status = CW_SYS_ERR; goto foundhandler; }

	struct CWG_script_pack sp;
	init_CWG_script_pack(&sp, &scriptStreams, name, revTxid, revision);
	if ((status = execScript(&sp, params, fd)) == CWG_SCRIPT_NO) { status = CW_OK; }

	// this should have been set NULL if anything was written from script execution; if not, it's deemed a bad script
	if (status == CW_OK && params->foundHandler != NULL) { status = CWG_SCRIPT_ERR; }

	foundhandler:
	if (status == params->foundSuppressErr) { status = CW_OK; }
	if (params->foundHandler != NULL) { params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL; }

	removeAllNodes(&scriptStreams, false);
	if (scriptStream) { fclose(scriptStream); }
	return status;
}

static CW_STATUS getFileByTxid(const char *txid, struct CWG_params *params, int fd) {
	CW_STATUS status;

	char hexDataStart[CW_TX_DATA_CHARS+1];
	struct CW_file_metadata md;

	if ((status = fetchHexData((const char **)&txid, 1, BY_TXID, params, NULL, hexDataStart)) != CW_OK) { goto foundhandler; }
	if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { goto foundhandler; }
	protocolCheck(md.pVer);	

	if (params->dirPath && md.type == CW_T_DIR) {
		FILE *dirFp;
		if ((dirFp = params->saveDirFp) == NULL &&
		    (dirFp = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto foundhandler; }

		if ((status = traverseFile(hexDataStart, params, &md, fileno(dirFp))) != CW_OK) {
			if (!params->saveDirFp) { fclose(dirFp); }
			goto foundhandler;
		}		
		rewind(dirFp);

		char pathTxid[CW_TXID_CHARS+1];
		if ((status = CWG_dir_path_to_identifier(dirFp, params->dirPath, pathTxid)) != CW_OK) {
			if (!params->saveDirFp) { fclose(dirFp); }
			goto foundhandler;
		}

		char *savePtrC = params->dirPath;
		params->dirPath = NULL;
		FILE *savePtrF = params->saveDirFp;
		params->saveDirFp = NULL;

		status = getFileByTxid(pathTxid, params, fd);	

		params->dirPath = savePtrC;
		params->saveDirFp = savePtrF;

		return status;
	} else if (params->dirPath) { status = CWG_IS_DIR_NO; goto foundhandler; }

	if (params->saveMimeStr && !strlen(*params->saveMimeStr)) {
		if ((status = cwTypeToMimeStr(md.type, params)) != CW_OK) { goto foundhandler; }
	}

	foundhandler:
	if (status == params->foundSuppressErr) { status = CW_OK; }
	if (params->foundHandler != NULL) { params->foundHandler(status, params->foundHandleData, fd); params->foundHandler = NULL; }
	if (status != CW_OK) { return status; }

	return traverseFile(hexDataStart, params, &md, fd);
}

static CW_STATUS initFetcher(struct CWG_params *params) {
	if (params->mongodb) {
		mongoc_init();
		bson_error_t error;	
		mongoc_uri_t *uri;
		if (!(uri = mongoc_uri_new_with_error(params->mongodb, &error))) {
			fprintf(stderr, "ERROR: cashgettools failed to parse provided MongoDB URI: %s\nMessage: %s\n", params->mongodb, error.message);
			return CW_SYS_ERR;
		}
		params->mongodbCli = mongoc_client_new_from_uri(uri);
		mongoc_uri_destroy(uri);	
		if (!params->mongodbCli) {
			fprintf(stderr, "ERROR: cashgettools failed to establish client with MongoDB\n");
			return CWG_FETCH_ERR;
		}
		mongoc_client_set_appname(params->mongodbCli, "cashgettools");
	} 
	else if (params->bitdbNode) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		if (params->bitdbRequestLimit) { srandom(time(NULL)); }
	}	
	else if (!params->mongodbCli)  {
		fprintf(stderr, "ERROR: cashgettools requires either MongoDB or BitDB Node address to be specified\n");
		return CW_SYS_ERR;
	}

	return CW_OK;
}

static void cleanupFetcher(struct CWG_params *params) {
	if (params->mongodb && params->mongodbCli) {
		mongoc_client_destroy(params->mongodbCli);
		params->mongodbCli = NULL;
		mongoc_cleanup();
	} 
	else if (params->bitdbNode) {
		curl_global_cleanup();
	}
}
