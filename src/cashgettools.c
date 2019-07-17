#include "cashgettools.h"

// MongoDB #defines
#define MONGODB_STR_HEX_PREFIX "OP_RETURN "

// BitDB HTTP #defines
#define BITDB_API_VER 3
#define BITDB_QUERY_LEN (71+strlen(BITDB_QUERY_DATA_TAG)+strlen(BITDB_QUERY_TXID_TAG))
#define BITDB_TXID_QUERY_LEN (12+TXID_CHARS)
#define BITDB_HEADER_BUF_SZ 40
#define BITDB_QUERY_DATA_TAG "data"
#define BITDB_RESPONSE_DATA_TAG "\""BITDB_QUERY_DATA_TAG"\":\""
#define BITDB_QUERY_TXID_TAG "txid"
#define BITDB_RESPONSE_TXID_TAG "\""BITDB_QUERY_TXID_TAG"\":\""

#define DIR_LINE_BUF 500

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
	uint16_t uint16 = 0; uint32_t uint32 = 0;
	bool isShort = false;
	switch (numBytes) {
		case sizeof(uint16_t):
			isShort = true;
			break;
		case sizeof(uint32_t):
			break;
		default:
			fprintf(stderr, "unsupported number of bytes read for network integer value; probably problem with cashgettools\n");
			return CWG_SYS_ERR;
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

	if (netHexStrToInt(chainLenHex, CW_MD_BYTES(length), &md->length) != CW_OK ||
	    netHexStrToInt(treeDepthHex, CW_MD_BYTES(depth), &md->depth) != CW_OK ||
	    netHexStrToInt(fTypeHex, CW_MD_BYTES(type), &md->type) != CW_OK ||
	    netHexStrToInt(pVerHex, CW_MD_BYTES(pVer), &md->pVer) != CW_OK) { return CWG_METADATA_NO; }

	return CW_OK;
}

/*
 * writes curl response to specified file stream
 */
static size_t writeResponseToFile(void *data, size_t size, size_t nmemb, FILE *respFp) {
	return fwrite(data, size, nmemb, respFp)*size;
}

/*
 * fetches hex data (from BitDB HTTP endpoint) at specified txids and copies (in order of txids) to specified location in memory 
 */
static CW_STATUS fetchHexDataBitDBNode(char *hexDataAll, const char *bitdbNode, bool bitdbRequestLimit, const char **txids, int count) {
	if (count < 1) { return CWG_FETCH_NO; }

	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() failed\n"); return CWG_FETCH_ERR; }

	// construct query
	char txidQuery[(BITDB_TXID_QUERY_LEN*count)+1];
	for (int i=0; i<count; i++) {
		snprintf(txidQuery + (i*BITDB_TXID_QUERY_LEN), BITDB_TXID_QUERY_LEN+1, "{\"tx.h\":\"%s\"},", txids[i]);
	}
	txidQuery[strlen(txidQuery)-1] = 0;
	char query[BITDB_QUERY_LEN + strlen(txidQuery) + 1];
	snprintf(query, sizeof(query), 
	"{\"v\":%d,\"q\":{\"find\":{\"$or\":[%s]}},\"r\":{\"f\":\"[.[]|{%s:.out[0].h1,%s:.tx.h}]\"}}",
		BITDB_API_VER, txidQuery, BITDB_QUERY_DATA_TAG, BITDB_QUERY_TXID_TAG);
	char *queryB64;
	if ((queryB64 = b64_encode((const unsigned char *)query, strlen(query))) == NULL) { perror("b64 encode failed");
											    curl_easy_cleanup(curl); return CWG_SYS_ERR; }
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
	if (respFp == NULL) { perror("tmpfile() failed"); curl_easy_cleanup(curl); return CWG_SYS_ERR; }
	struct curl_slist *headers = NULL;
	if (bitdbRequestLimit) { // this bit is to trick a server's request limit, although won't necessarily work with every server
		char buf[BITDB_HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d",
			rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeResponseToFile);
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
			CW_STATUS status1 = fetchHexDataBitDBNode(hexDataAll, bitdbNode, bitdbRequestLimit, txids, firstCount);
			CW_STATUS status2 = fetchHexDataBitDBNode(hexDataAll+strlen(hexDataAll), bitdbNode, bitdbRequestLimit, txids+firstCount, count-firstCount);
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
			status = CWG_SYS_ERR;
			goto cleanup;
		}
	}

	// parse for hex datas at matching txids within both unconfirmed and confirmed transaction json arrays
	json_t *jsonArrs[2] = { json_object_get(respJson, "c"), json_object_get(respJson, "u") };
	size_t index;
	json_t *dataJson;
	const char *dataTxid;
	const char *dataHex;
	bool matched;
	memset(added, 0, count);
	for (int i=0; i<count; i++) {
		matched = false;
		for (int a=0; a<sizeof(jsonArrs)/sizeof(jsonArrs[0]); a++) {
			if (!jsonArrs[a]) {
				fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", json_dumps(respJson, 0));
				status = CWG_FETCH_ERR;
				goto cleanup;
			}
			
			json_array_foreach(jsonArrs[a], index, dataJson) {
				if ((dataTxid = json_string_value(json_object_get(dataJson, BITDB_QUERY_TXID_TAG))) == NULL ||
				    (dataHex = json_string_value(json_object_get(dataJson, BITDB_QUERY_DATA_TAG))) == NULL) {
					fprintf(stderr, "BitDB node responded with unexpected JSON format:\n%s\n", json_dumps(jsonArrs[a], 0));
					status = CWG_FETCH_ERR; goto cleanup;
				}
				if (!added[i] && strcmp(txids[i], dataTxid) == 0) {
					hexDataPtrs[i] = dataHex;	
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
	for (int i=0; i<count; i++) { strncat(hexDataAll, hexDataPtrs[i], TX_DATA_CHARS); }

	cleanup:
		json_decref(respJson);	
		fclose(respFp);
		return status;
}

/*
 * fetches hex data (from MongoDB populated by BitDB) at specified txids and copies (in order of txids) to specified location in memory 
 */
static CW_STATUS fetchHexDataMongoDB(char *hexDataAll, mongoc_client_t *mongodbCli, const char **txids, int count) {
	if (count < 1) { return CWG_FETCH_NO; }
	CW_STATUS status = CW_OK;
	
	hexDataAll[0] = 0;
	mongoc_collection_t *colls[2] = { mongoc_client_get_collection(mongodbCli, "bitdb", "confirmed"), 
					  mongoc_client_get_collection(mongodbCli, "bitdb", "unconfirmed") };
	bson_t *query;
	bson_t *opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "_id", BCON_BOOL(false), "}");
	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *res;
	char *resStr;
	json_t *resJson;
	json_error_t jsonError;
	const char *hexData;
	int hexPrefixLen = strlen(MONGODB_STR_HEX_PREFIX);
	bool matched;
	for (int i=0; i<count; i++) { 
		matched = false;
		query = BCON_NEW("tx.h", txids[i]);
		for (int c=0; c<sizeof(colls)/sizeof(colls[0]); c++) {
			cursor = mongoc_collection_find_with_opts(colls[c], query, opts, NULL);
			if (!mongoc_cursor_next(cursor, &res)) {
				if (mongoc_cursor_error(cursor, &error)) {
					fprintf(stderr, "ERROR: MongoDB query failed\nMessage: %s\n", error.message);
					status = CWG_FETCH_ERR;
				} 
				mongoc_cursor_destroy(cursor);
				if (status != CW_OK) { break; } else { continue; } 
			}
			resStr = bson_as_canonical_extended_json(res, NULL);
			resJson = json_loads(resStr, JSON_ALLOW_NUL, &jsonError);
			bson_free(resStr);
			mongoc_cursor_destroy(cursor);
			if (resJson == NULL) {
				fprintf(stderr, "jansson error in parsing result from MongoDB query: %s\nResponse:\n%s\n", jsonError.text, resStr);
				status = CWG_SYS_ERR;
				break;
			}
			// gets json array at key 'out' -> json object at array index 0 -> json object at key 'str' (.out[0].str)
			hexData = json_string_value(json_object_get(json_array_get(json_object_get(resJson, "out"), 0), "str"));
			if (strncmp(hexData, MONGODB_STR_HEX_PREFIX, hexPrefixLen) == 0) {
				strncat(hexDataAll, hexData+hexPrefixLen, TX_DATA_CHARS);
				matched = true;
			} else { status = CWG_FILE_ERR; }
			break;
		}
		bson_destroy(query);
		if (!matched) { status = status == CW_OK ? CWG_FETCH_NO : status; break; }
	}
	bson_destroy(opts);

	return status;
}

static inline CW_STATUS fetchHexData(char *hexDataAll, const char **txids, int count, struct CWG_params *params) {
	if (params->mongodbCli) { return fetchHexDataMongoDB(hexDataAll, params->mongodbCli, txids, count); }
	else if (params->bitdbNode) { return fetchHexDataBitDBNode(hexDataAll, params->bitdbNode, params->bitdbRequestLimit, txids, count); }
	else {
		fprintf(stderr, "ERROR: neither MongoDB nor BitDB HTTP endpoint address is set in cashgettools implementation\n");
		return CWG_SYS_ERR;
	}
}

static CW_STATUS traverseFileTree(const char *treeHexData, List *partialTxids[], int suffixLen, int depth,
			    	  struct CWG_params *params, struct CW_file_metadata *md, int fd) {

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
	if ((status = fetchHexData(hexDataAll, (const char **)txids, txidsCount, params)) == CW_OK) { 
		if (partialTxids != NULL) {
			char *partialTxidN = malloc(TXID_CHARS+1);
			if (partialTxidN == NULL) { perror("malloc failed"); status = CWG_SYS_ERR; goto cleanup; }
			strcpy(partialTxidN, partialTemp);
			addFront(partialTxids[1], partialTxidN);
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
			if ((status = fetchHexData(hexDataNext, (const char **)&txidNext, 1, params)) == CWG_FETCH_NO) {
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
		removeAllNodes(partialTxids[0]); removeAllNodes(partialTxids[1]);
		free(txidNext);
		free(hexDataNext);
		return status;
}

static inline CW_STATUS traverseFile(const char *hexDataStart, struct CWG_params *params, struct CW_file_metadata *md, int fd) {
	return md->length > 0 || md->depth == 0 ? traverseFileChain(hexDataStart, params, md, fd)
						: traverseFileTree(hexDataStart, NULL, CW_METADATA_CHARS, 0, params, md, fd);
}

static CW_STATUS getFileByTxid(const char *txid, struct CWG_params *params, int fd) {
	CW_STATUS status;

	char *hexDataStart = malloc(TX_DATA_CHARS+1);
	if (hexDataStart == NULL) { perror("malloc failed"); status = CWG_SYS_ERR; goto foundhandler; }
	struct CW_file_metadata md;

	if ((status = fetchHexData(hexDataStart, (const char **)&txid, 1, params)) != CW_OK) { goto foundhandler; }
	if ((status = hexResolveMetadata(hexDataStart, &md)) != CW_OK) { goto foundhandler; }
	protocolCheck(md.pVer);
	if (params->dirPath && md.type == CW_T_DIR) {
		FILE *dirFp;
		if ((dirFp = params->saveDirFp) == NULL &&
		    (dirFp = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CWG_SYS_ERR; goto foundhandler; }

		if ((status = traverseFile(hexDataStart, params, &md, fileno(dirFp))) != CW_OK) { goto foundhandler; }		
		rewind(dirFp);

		char pathTxid[TXID_CHARS+1];
		if ((status = CWG_dir_path_to_identifier(dirFp, params->dirPath, pathTxid)) != CW_OK) { goto foundhandler; }

		struct CWG_params dirFileParams;
		copy_CWG_params(&dirFileParams, params);
		dirFileParams.dirPath = NULL;
		dirFileParams.saveDirFp = NULL;

		status = getFileByTxid(pathTxid, &dirFileParams, fd);
		goto cleanup;
	} else if (params->dirPath) { status = CWG_IS_DIR_NO; }

	foundhandler:
	if (status == params->foundSuppressErr) { status = CW_OK; }
	if (params->foundHandler != NULL) { params->foundHandler(status, params->foundHandleData, fd); }
	if (status != CW_OK) { goto cleanup; }

	status = traverseFile(hexDataStart, params, &md, fd);
	
	cleanup:
		free(hexDataStart);
		return status;
}

static CW_STATUS initParams(struct CWG_params *params) {
	if (params->mongodb) {
		mongoc_init();
		bson_error_t error;	
		mongoc_uri_t *uri;
		if (!(uri = mongoc_uri_new_with_error(params->mongodb, &error))) {
			fprintf(stderr, "ERROR: cashgettools failed to parse provided MongoDB URI: %s\nMessage: %s\n", params->mongodb, error.message);
			return CWG_SYS_ERR;
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
		return CWG_SYS_ERR;
	}

	return CW_OK;
}

static void cleanupParams(struct CWG_params *params) {
	if (params->mongodb && params->mongodbCli) {
		mongoc_client_destroy(params->mongodbCli);
		params->mongodbCli = NULL;
		mongoc_cleanup();
	} 
	else if (params->bitdbNode) {
		curl_global_cleanup();
	}
}

/*
 * see non-static function descriptions in header file
 */
CW_STATUS CWG_get_by_txid(const char *txid, struct CWG_params *params, int fd) {
	CW_STATUS status;
	if ((status = initParams(params)) != CW_OK) { return status; } 

	status = getFileByTxid(txid, params, fd);
	
	cleanupParams(params);
	return status;
}

CW_STATUS CWG_dir_path_to_identifier(FILE *dirFp, const char *dirPath, char *pathTxid) {
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
	if (ferror(dirFp)) { perror("error reading dirFp in CWG_dir_path_to_identifier()"); status = CWG_SYS_ERR; }
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

const char *CWG_errno_to_msg(int errNo) {
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
