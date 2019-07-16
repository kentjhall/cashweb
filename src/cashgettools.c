#include "cashgettools.h"

#define QUERY_LEN (71+strlen(QUERY_DATA_TAG)+strlen(QUERY_TXID_TAG))
#define TXID_QUERY_LEN (12+TXID_CHARS)
#define HEADER_BUF_SZ 35
#define QUERY_DATA_TAG "data"
#define RESPONSE_DATA_TAG "\""QUERY_DATA_TAG"\":\""
#define QUERY_TXID_TAG "txid"
#define RESPONSE_TXID_TAG "\""QUERY_TXID_TAG"\":\""

#define DIR_LINE_BUF 500

#define BITDB_API_VER 3

static mongoc_client_t *mongodbCli = NULL;
static const char *bitdbNode = NULL;
static bool bitdbRequestLimit = true;

/* 
 * currently, simply reports a warning if given protocol version is newer than one in use
 * may be made more robust in the future
 */
static void protocolCheck(int pVer) {
	if (pVer > CW_P_VER) {
		fprintf(stderr, "WARNING: requested file signals a newer cashweb protocol version than this client uses (client: CWP %d, file: CWP %d).\nWill attempt to read anyway, in case this is inaccurate or the protocol upgrade is trivial.\nIf there is a new protocol version available, it is recommended you upgrade.\n", CW_P_VER, pVer);
	}
}

/*
 * translates given hex string to byte data and writes to file descriptor
 */
static CW_STATUS writeHexDataStr(const char *hexDataStr, int suffixLen, int fd) {
	char fileByteData[strlen(hexDataStr)/2];
	int bytesToWrite;

	if ((bytesToWrite = hexStrToByteArr(hexDataStr, suffixLen, fileByteData)) < 0) {
		return CWG_FILE_ERR;
	}
	if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) {
		perror("write() failed"); 
		return CWG_WRITE_ERR;
	}
	
	return CW_OK;
}

/* 
 * resolves array of bytes into a network byte order (big-endian) unsigned integer,
 * and then converts to host byte order
 * must make sure void* is appropriate unsigned integer type (uint16_t or uint32_t),
 * and passed hex must be appropriate length
 */
static CW_STATUS netHexStrToInt(const char *hex, int numBytes, void *uintPtr) {
	if (numBytes != strlen(hex)/2) { return CWG_FILE_ERR; }
	uint16_t uint16; uint32_t uint32;
	bool isShort = false;
	switch (numBytes) {
		case sizeof(uint16_t):
			isShort = true;
			uint16 = 0;
			break;
		case sizeof(uint32_t):
			uint32 = 0;
			break;
		default:
			fprintf(stderr, "unsupported number of bytes read for network integer value; probably problem with cashgettools\n");
			return CWG_FILE_ERR;
	}

	char byteData[numBytes];
	if (hexStrToByteArr(hex, 0, byteData) < 0) { fprintf(stderr, "invalid hex data passed for network integer value; probably problem with cashgettools\n"); return CWG_FILE_ERR; }

	for (int i=0; i<numBytes; i++) {
		if (isShort) { uint16 |= (uint16_t)byteData[i] << i*8; } else { uint32 |= (uint32_t)byteData[i] << i*8; }
	}
	if (isShort) { *(uint16_t *)uintPtr = ntohs(uint16); }
	else { *(uint32_t *)uintPtr = ntohl(uint32); }

	return CW_OK;
}

/*
 * resolves file metadata from given hex data string according to protocol format,
 * and save to given struct pointer
 */
static CW_STATUS hexResolveMetadata(const char *hexData, struct cwFileMetadata *md) {
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

	if (netHexStrToInt(chainLenHex, CW_MD_BYTES(length), &md->length) != CW_OK ||
	    netHexStrToInt(treeDepthHex, CW_MD_BYTES(depth), &md->depth) != CW_OK ||
	    netHexStrToInt(fTypeHex, CW_MD_BYTES(type), &md->type) != CW_OK ||
	    netHexStrToInt(pVerHex, CW_MD_BYTES(pVer), &md->pVer) != CW_OK) { return CWG_METADATA_NO; }

	return CW_OK;
}

/*
 * writes curl response to specified file stream
 */
static size_t copyResponseToMemory(void *data, size_t size, size_t nmemb, FILE *respFp) {
	return fwrite(data, size, nmemb, respFp)*size;
}

/*
 * parses given json array for hex datas and puts in appropriate memory location based on matching txid
 * only exists as separate function because it is used multiple times within fetchHexData()
 */
static CW_STATUS parseJsonArrayForHexDatas(json_t *jsonArr, int count, const char **txids, const char *hexDataPtrs[]) {
	size_t index;
	json_t *dataJson;
	const char *dataTxid;
	const char *dataHex;
	bool added[count]; memset(added, 0, count);
	bool matched;
	json_array_foreach(jsonArr, index, dataJson) {
		if ((dataTxid = json_string_value(json_object_get(dataJson, QUERY_TXID_TAG))) == NULL ||
		    (dataHex = json_string_value(json_object_get(dataJson, QUERY_DATA_TAG))) == NULL) {
			fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", json_dumps(jsonArr, 0));
			return CWG_FETCH_ERR;
		}
		matched = false;
		for (int i=0; i<count; i++) {
			if (!added[i] && strcmp(txids[i], dataTxid) == 0) {
				hexDataPtrs[i] = dataHex;	
				added[i] = true;
				matched = true;
				break;
			}
		}
		if (!matched) { return CWG_FETCH_NO; }
	}
	return CW_OK;
}

/*
 * fetches hex data (from BitDB HTTP endpoint) at specified txids and copies (in order of txids) to specified location in memory 
 */
static CW_STATUS fetchHexDataBitDBNode(char *hexDataAll, const char **txids, int count) {
	if (bitdbNode == NULL) { fprintf(stderr, "BitDB Node address not set; problem with cashgettools implementation\n");
				 return CWG_SYS_ERR; }
	if (count < 1) { return CWG_FETCH_NO; }

	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() failed\n"); return CWG_FETCH_ERR; }

	// construct query
	char txidQuery[(TXID_QUERY_LEN*count)+1];
	for (int i=0; i<count; i++) {
		snprintf(txidQuery + (i*TXID_QUERY_LEN), TXID_QUERY_LEN+1, "{\"tx.h\":\"%s\"},", txids[i]);
	}
	txidQuery[strlen(txidQuery)-1] = 0;
	char query[QUERY_LEN + strlen(txidQuery) + 1];
	snprintf(query, sizeof(query), 
	"{\"v\":%d,\"q\":{\"find\":{\"$or\":[%s]}},\"r\":{\"f\":\"[.[]|{%s:.out[0].h1,%s:.tx.h}]\"}}",
		BITDB_API_VER, txidQuery, QUERY_DATA_TAG, QUERY_TXID_TAG);
	char *queryB64;
	if ((queryB64 = b64_encode((const unsigned char *)query, strlen(query))) == NULL) { perror("b64 encode failed");
											    curl_easy_cleanup(curl); return CWG_SYS_ERR; }
	char url[strlen(bitdbNode) + strlen(queryB64) + 1 + 1];

	// construct url from query
	strcpy(url, bitdbNode);
	strcat(url, "/");
	strcat(url, queryB64);
	free(queryB64);	

	// initializing variable-length array for hex data pointers before goto statements
	const char *hexDataPtrs[count];

	// send curl request
	FILE *respFp = tmpfile();
	if (respFp == NULL) { perror("tmpfile() failed"); curl_easy_cleanup(curl); return CWG_SYS_ERR; }
	struct curl_slist *headers = NULL;
	if (bitdbRequestLimit) { // this bit is to trick a server's request limit, although won't necessarily work with every server
		char buf[HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d",
			rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &copyResponseToMemory);
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
		char respMsg[respSz];
		respMsg[fread(respMsg, 1, respSz, respFp)] = 0;
		if ((strlen(respMsg) < 1 && count > 1) || (strstr(respMsg, "URI") && strstr(respMsg, "414"))) { // catch for Request-URI Too Large or empty response body
			int firstCount = count/2;
			CW_STATUS status1; CW_STATUS status2;
			if ((status1 = fetchHexDataBitDBNode(hexDataAll, txids, firstCount)) == 
			    (status2 = fetchHexDataBitDBNode(hexDataAll+strlen(hexDataAll), txids+firstCount, count-firstCount))) { status = status1; }
			else { status = status1 > status2 ? status1 : status2; }
			goto cleanup;
		}
		else if (strstr(respMsg, "html")) {
			fprintf(stderr, "HTML response error unhandled in cashgettools:\n%s\n", respMsg);
			status = CWG_FETCH_ERR;
			goto cleanup;
		}
		else {
			fprintf(stderr, "jansson error in parsing response from BitDB node: %s\nResponse:\n%s\n", jsonError.text, respMsg);
			status = CWG_FETCH_ERR;
			goto cleanup;
		}
	}

	// parse for hex datas at matching txids within both unconfirmed and confirmed transaction json arrays
	json_t *uRespArr = json_object_get(respJson, "u");
	json_t *cRespArr = json_object_get(respJson, "c");
	if (!uRespArr || !cRespArr) {
		fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", json_dumps(respJson, 0));
		status = CWG_FETCH_ERR;
		goto cleanup;
	}
	if ((status = parseJsonArrayForHexDatas(uRespArr, count, txids, hexDataPtrs)) != CW_OK ||
	    (status = parseJsonArrayForHexDatas(cRespArr, count, txids, hexDataPtrs)) != CW_OK) { goto cleanup; }
	
	hexDataAll[0] = 0;
	for (int i=0; i<count; i++) { strncat(hexDataAll, hexDataPtrs[i], TX_DATA_CHARS); }

	cleanup:
		json_decref(respJson);	
		fclose(respFp);
		return status;
}

/*
 * fetches hex data (from MongoDB populated by BitDB) at specified txids and copies (in order of txids) to specified location in memory 
 */
static CW_STATUS fetchHexDataMongoDB(char *hexDataAll, const char **txids, int count) {
	if (mongodbCli == NULL) { fprintf(stderr, "MongoDB address not set; problem with cashgettools implementation\n");
				 return CWG_SYS_ERR; }

	
}

static inline CW_STATUS fetchHexData(char *hexDataAll, const char **txids, int count) {
	if (mongodbCli) { return fetchHexDataMongoDB(hexDataAll, txids, count); }
	else if (bitdbNode) { return fetchHexDataBitDBNode(hexDataAll, txids, count); }
	else {
		fprintf(stderr, "ERROR: neither MongoDB nor BitDB HTTP endpoint address is set for cashgettools\n");
		return CWG_SYS_ERR;
	}
}

static CW_STATUS traverseFileTree(const char *treeHexData, List *partialTxids[], int suffixLen, int depth,
			    struct cwFileMetadata *md, int fd) {

	char *partialTxid;
	int partialTxidFill = partialTxids != NULL && (partialTxid = popFront(partialTxids[0])) != NULL ?
			      TXID_CHARS-strlen(partialTxid) : 0;	
	
	int numChars = strlen(treeHexData+partialTxidFill)-suffixLen;
	int txidsCount = numChars/TXID_CHARS + (partialTxidFill ? 1 : 0);
	if (txidsCount < 1) { return CW_OK; }

	CW_STATUS status = CW_OK;

	char *txids[txidsCount];
	for (int i=0; i<txidsCount; i++) {
		if ((txids[i] = malloc(TXID_CHARS+1)) == NULL) { perror("malloc failed"); status  = CWG_SYS_ERR; }
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
		strncpy(txids[i], txidPtr, TXID_CHARS);
		txids[i][TXID_CHARS] = 0;
		txidPtr += TXID_CHARS;
	}
	char partialTemp[TXID_CHARS+1]; partialTemp[0] = 0;
	strncat(partialTemp, txidPtr, numChars-(txidPtr-(treeHexData+partialTxidFill)));

	// frees the tree hex data if in recursive call
	if (depth > 0) { free((char *)treeHexData); }

	char *hexDataAll = malloc(TX_DATA_CHARS*txidsCount + 1);
	if (hexDataAll == NULL) { perror("malloc failed"); status =  CWG_SYS_ERR; goto cleanup; }
	if ((status = fetchHexData(hexDataAll, (const char **)txids, txidsCount)) == CW_OK) { 
		if (partialTxids != NULL) {
			char *partialTxidN = malloc(TXID_CHARS+1);
			if (partialTxidN == NULL) { perror("malloc failed"); status = CWG_SYS_ERR; goto cleanup; }
			strcpy(partialTxidN, partialTemp);
			addFront(partialTxids[1], partialTxidN);
		}

		if (depth+1 < md->depth) {
			status = traverseFileTree(hexDataAll, partialTxids, 0, depth+1, md, fd);
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

static CW_STATUS traverseFileChain(const char *hexDataStart, struct cwFileMetadata *md, int fd) {
	char hexData[TX_DATA_CHARS+1];
	strcpy(hexData, hexDataStart);
	char *hexDataNext = malloc(TX_DATA_CHARS+1);
	if (hexDataNext == NULL) { perror("malloc failed"); return CWG_SYS_ERR; }
	char *txidNext = malloc(TXID_CHARS+1);
	if (txidNext == NULL) { perror("malloc failed"); free(hexDataNext); return CWG_SYS_ERR; }

	List partialTxidsO; List partialTxidsN;
	List *partialTxids[2] = { &partialTxidsO, &partialTxidsN };
	initList(partialTxids[0]); initList(partialTxids[1]);

	CW_STATUS status;
	int suffixLen;
	bool end = false;
	for (int i=0; i <= md->length; i++) {
		if (i == 0) {
			suffixLen = CW_METADATA_CHARS;
			if (i < md->length) { suffixLen += TXID_CHARS; } else { end = true; }
		}
		else if (i == md->length) {
			suffixLen = 0;
			end = true;
		}
		else {
			suffixLen = TXID_CHARS;
		}
	
		if (strlen(hexData) < suffixLen) { status = CWG_FILE_ERR; goto cleanup; }
		if (!end) {
			strncpy(txidNext, hexData+(strlen(hexData) - suffixLen), TXID_CHARS);
			txidNext[TXID_CHARS] = 0;
			if ((status = fetchHexData(hexDataNext, (const char **)&txidNext, 1)) == CWG_FETCH_NO) {
				status = CWG_FILE_LEN_ERR;	
				goto cleanup;
			} else if (status != CW_OK) { goto cleanup; }
		}

		if (!md->depth) {
			if ((status = writeHexDataStr(hexData, suffixLen, fd)) != CW_OK) { goto cleanup; }
		} else {
			if ((status = traverseFileTree(hexData, partialTxids, suffixLen, 0, md, fd)) != CW_OK) {
				goto cleanup;
			}
		} 
		strcpy(hexData, hexDataNext);
	}
	
	cleanup:
		removeAllNodes(partialTxids[0]); removeAllNodes(partialTxids[1]);
		free(txidNext);
		free(hexDataNext);
		return status;
}

static inline CW_STATUS traverseFile(const char *hexDataStart, struct cwFileMetadata *md, int fd) {
	return md->length > 0 || md->depth == 0 ? traverseFileChain(hexDataStart, md, fd)
						: traverseFileTree(hexDataStart, NULL, CW_METADATA_CHARS, 0, md, fd);
}

static CW_STATUS getFileByTxid(const char *txid, struct cwGetParams *params, int fd) {
	char *hexDataStart = malloc(TX_DATA_CHARS+1);
	if (hexDataStart == NULL) { perror("malloc failed"); goto foundhandler; }
	struct cwFileMetadata md;

	CW_STATUS status;
	if ((status = fetchHexData(hexDataStart, (const char **)&txid, 1)) != CW_OK) { goto foundhandler; }
	if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { goto foundhandler; }
	protocolCheck(md.pVer);
	if (params->dirPath && md.type == CW_T_DIR) {
		FILE *dirFp;
		if ((dirFp = params->saveDirFp) == NULL &&
		    (dirFp = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CWG_SYS_ERR; goto foundhandler; }

		if ((status = traverseFile(hexDataStart, &md, fileno(dirFp))) != CW_OK) { goto foundhandler; }		
		rewind(dirFp);

		char pathTxid[TXID_CHARS+1];
		if ((status = dirPathToTxid(dirFp, params->dirPath, pathTxid)) != CW_OK) { goto foundhandler; }

		struct cwGetParams dirFileParams;
		copyCwGetParams(&dirFileParams, params);
		dirFileParams.dirPath = NULL;
		dirFileParams.saveDirFp = NULL;

		status = getFileByTxid(pathTxid, &dirFileParams, fd);
		goto cleanup;
	} else if (params->dirPath) { status = CWG_IS_DIR_NO; }

	foundhandler:
	if (status == params->foundSuppressErr) { status = CW_OK; }
	if (params->foundHandler != NULL) { params->foundHandler(status, params->foundHandleData, fd); }
	if (status != CW_OK) { goto cleanup; }

	status = traverseFile(hexDataStart, &md, fd);
	
	cleanup:
		free(hexDataStart);
		return status;
}

static CW_STATUS initGlobals(struct cwGetParams *params) {
	if (params->mongodb) {
		mongoc_init();
		bson_error_t error;	
		mongoc_uri_t *uri;
		if (!(uri = mongoc_uri_new_with_error(params->mongodb, &error))) {
			fprintf(stderr, "ERROR: cashgettools failed to parse provided MongoDB URI: %s\nMessage: %s\n", params->mongodb, error.message);
			return CWG_SYS_ERR;
		}
		mongodbCli = mongoc_client_new_from_uri(uri);
		mongoc_uri_destroy(uri);	
		if (!mongodbCli) {
			fprintf(stderr, "ERROR: cashgettools failed to establish client with MongoDB\n");
			return CWG_FETCH_ERR;
		}
		mongoc_client_set_appname(mongodbCli, "cashgettools");
	} 
	else if (params->bitdbNode) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		bitdbNode = params->bitdbNode;
		bitdbRequestLimit = params->bitdbRequestLimit;
	}	
	else { fprintf(stderr, "ERROR: cashgettools requires either MongoDB or BitDB Node address to be specified\n"); return CWG_SYS_ERR; }
	if (bitdbNode && bitdbRequestLimit) { srandom(time(NULL)); }

	return CW_OK;
}

static void cleanupGlobals() {
	if (mongodbCli) {
		mongoc_client_destroy(mongodbCli);
		mongoc_cleanup();
	} 
	if (bitdbNode) {
		curl_global_cleanup();
	}
}

/*
 * see non-static function descriptions in header file
 */
CW_STATUS getFile(const char *txid, struct cwGetParams *params, int fd) {
	CW_STATUS status;
	if ((status = initGlobals(params)) != CW_OK) { return status; } 

	status = getFileByTxid(txid, params, fd);
	
	cleanupGlobals();
	return status;
}

CW_STATUS dirPathToTxid(FILE *dirFp, const char *dirPath, char *pathTxid) {
	CW_STATUS status = CWG_IN_DIR_NO;

	char pathTxidBytes[TXID_BYTES];
	memset(pathTxidBytes, 0, TXID_BYTES);

	struct DynamicMemory line;
	initDynamicMemory(&line);
	resizeDynamicMemory(&line, DIR_LINE_BUF);
	bzero(line.data, line.size);

	bool found = false;
	int count = 0;
	int lineLen;
	int offset = 0;
	while (fgets(line.data+offset, line.size-1-offset, dirFp) != NULL) {
		lineLen = strlen(line.data);
		if (line.data[lineLen-1] != '\n' && !feof(dirFp)) {
			offset = lineLen;
			resizeDynamicMemory(&line, line.size*2);
			bzero(line.data+offset, line.size-offset);
			continue;
		}
		else if (offset > 0) { offset = 0; }
		line.data[lineLen-1] = 0;

		if (strlen(line.data) < 1) { break; }
				
		if (!found) {
			++count;
			if (strcmp(line.data, dirPath) == 0) {
				found = true;
				status = CW_OK;
			}
		}
	}
	if (ferror(dirFp)) { perror("error reading dirFp in dirPathToTxid()"); status = CWG_SYS_ERR; }
	if (status != CW_OK) { goto cleanup; }

	if (count > 0) {
		if (fseek(dirFp, TXID_BYTES*(count-1), SEEK_CUR) < 0) { perror("fseek() SEEK_CUR failed"); status = CWG_SYS_ERR; goto cleanup; }
		if (fread(pathTxidBytes, TXID_BYTES, 1, dirFp) < 1) { perror("fread() failed on dirFp"); status = CWG_SYS_ERR; goto cleanup; }
		byteArrToHexStr(pathTxidBytes, TXID_BYTES, pathTxid);
	} else { status = CWG_IN_DIR_NO; }

	cleanup:
		freeDynamicMemory(&line);
		return status;
}

const char *cwgErrNoToMsg(int errNo) {
	switch (errNo) {
		case CWG_IN_DIR_NO:
			return "Requested file doesn't exist in specified directory";
		case CWG_IS_DIR_NO:
			return "Requested file isn't a directory";
		case CWG_FETCH_NO:
			return "Requested file doesn't exist, check identifier";
		case CWG_METADATA_NO:
			return "Requested file's metadata is invalid or nonexistent, check identifier";
		case CWG_SYS_ERR:
			return "There was an unexpected system error; may be problem with cashgettools";
		case CWG_FETCH_ERR:
			return "There was an unexpected error in querying the blockchain";
		case CWG_WRITE_ERR:
			return "There was an unexpected error in writing the file";
		case CWG_FILE_LEN_ERR:
		case CWG_FILE_DEPTH_ERR:
		case CWG_FILE_ERR:
			return "There was an unexpected error in interpreting the file. The file may be encoded incorrectly (i.e. inaccurate metadata/structuring), or there is a problem with cashgettools";
		default:
			return "Received unexpected error code. This is likely an issue with cashgettools";
	}
}
