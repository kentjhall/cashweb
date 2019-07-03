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
#define IS_BITDB_REQUEST_LIMIT 1

static const char *bitdbNode;

struct DynamicMemory {
	char *data;
	size_t size;
};

char *errNoToMsg(int errNo) {
	switch (errNo) {
		case CWG_DIR_NO:
			return "Requested file doesn't exist in that directory.";
			break;
		case CWG_FETCH_NO:
			return "Requested file/directory doesn't exist, check identifier.";
			break;
		case CWG_FETCH_ERR:
			return "There was an unexpected error in querying the blockchain.";
			break;
		case CWG_WRITE_ERR:
			return "There was an unexpected error in writing the file.";
			break;
		case CWG_FILE_ERR:
			return "There was an unexpected error in interpreting the file; this is probably an issue with cashgettools.";
			break;
		default:
			return "Unexpected error code; this is probably an issue with cashgettools.";
	}
}

// converts hex bytes to chars, accounting for possible txid at end, and copies to specified memory loc
// returns number of bytes to write
static int hexStrDataToFileBytes(char *byteData, const char *hexData, int fileEnd) {
	if (strlen(hexData) % 2 != 0) { fprintf(stderr, "invalid hex data\n"); return 0; }

	int tailOmit = !fileEnd ? TXID_CHARS : 0;
	char hexByte[2+1];
	hexByte[2] = 0;
	for (int i=0; i<strlen(hexData)-tailOmit; i+=2) { 
		strncpy(hexByte, hexData+i, 2);
		byteData[i/2] = (char)strtoul(hexByte, NULL, 16);
	}
	return (int)(strlen(hexData)-tailOmit)/2;
}

// copies curl response to specified address in memory; needs to be freed
static size_t copyResponseToMemory(void *data, size_t size, size_t nmemb, struct DynamicMemory *dm) {
	size_t newSize = dm->size + (nmemb*size);
	if ((dm->data = realloc(dm->data, newSize + 1)) == NULL) { die("malloc failed"); }
	memcpy(dm->data + dm->size, data, newSize - dm->size);
	dm->size = newSize;
	return nmemb*size;
}

// fetches hex data at specified txid and copies to specified location in memory 
static int fetchHexData(char **hexDatas, const char **txids, int count) {
	if (count < 1) { return 1; }

	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() failed\n"); return CWG_FETCH_ERR; }

	int status = CWG_OK;

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
	if ((queryB64 = b64_encode((const unsigned char *)query, strlen(query))) == NULL) { die("b64 encode failed"); }

	// construct url from query
	char url[strlen(bitdbNode) + strlen(queryB64) + 1 + 1];
	strcpy(url, bitdbNode);
	strcat(url, "/");
	strcat(url, queryB64);
	free(queryB64);

	// initializing this variable-length array before goto; will track when a txid's hex data has already been saved
	int added[count]; 
	memset(added, 0, count*sizeof(added[0]));

	// send curl request
	struct DynamicMemory responseDm;
	responseDm.data = NULL;
	responseDm.size = 0;
	char *response = NULL;
	char *responsesParsed[count];
	if (IS_BITDB_REQUEST_LIMIT) { // this bit is to trick a server's request limit, although won't necessarily work with every server
		struct curl_slist *headers = NULL;
		char buf[HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d",
			rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &copyResponseToMemory);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseDm);
	if ((res = curl_easy_perform(curl)) != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		status = CWG_FETCH_ERR; goto cleanup;
	} 
	(responseDm.data)[responseDm.size] = 0;
	response = responseDm.data;
	if (strstr(response, "414 ")) { // catch for Request-URI Too Large
		int firstCount = count/2;
		int status1; int status2;
		if ((status1 = fetchHexData(hexDatas, txids, firstCount)) == 
		    (status2 = fetchHexData(hexDatas, txids+firstCount, count-firstCount))) { return status1; }
		else { return status1 > status2 ? status1 : status2; }
	}

	char *dataTxidPtr;
	char dataTxid[TXID_CHARS+1];
	int pos = 0;
	for (int i=0; i<count; i++) {
		// parse each response for hex data
		if ((responsesParsed[i] = strstr(response+pos, RESPONSE_DATA_TAG)) == NULL) { status = CWG_FETCH_NO; goto cleanup; }
		responsesParsed[i] += strlen(RESPONSE_DATA_TAG);
		pos = (responsesParsed[i] - response);
		for (int j=0; j<strlen(responsesParsed[i]); j++) {
			if (responsesParsed[i][j] == '"') { responsesParsed[i][j] = 0; pos += j+1; break; }
		}

		// copy each hex data to memory location by corresponding txid
		if ((dataTxidPtr = strstr(response+pos, RESPONSE_TXID_TAG)) == NULL) { status = CWG_FETCH_NO; goto cleanup; }
		dataTxidPtr += strlen(RESPONSE_TXID_TAG);
		strncpy(dataTxid, dataTxidPtr, TXID_CHARS);
		dataTxid[TXID_CHARS] = 0;

		int matched = 0;
                for (int j=0; j<count; j++) {
                        if (!added[j] && strcmp(dataTxid, txids[j]) == 0) {
                                strncpy(hexDatas[j], responsesParsed[i], TX_DATA_CHARS);
                                hexDatas[j][TX_DATA_CHARS] = 0;
                                added[j] = 1;
                                matched = 1;
                                break;
                        }
                }

		if (!matched) { status = CWG_FETCH_NO; goto cleanup; }
	}

	cleanup:
		if (response != NULL) { free(response); }
		curl_easy_cleanup(curl);
		return status;
}

static int traverseFileTree(const char *treeHexData, struct List *partialTxids[], int suffixLen, int fd) {
	char *partialTxid;
	int partialTxidFill = (partialTxid = popFront(partialTxids[0])) != NULL ? TXID_CHARS-strlen(partialTxid) : 0;	
	
	int numChars = strlen(treeHexData+partialTxidFill)-suffixLen;
	int txidsCount = numChars/TXID_CHARS + (partialTxidFill ? 1 : 0);

	char *txids[txidsCount];
	char *hexDatas[txidsCount];
	for (int i=0; i<txidsCount; i++) { if ((txids[i] = malloc(TXID_CHARS+1)) == NULL || 
					       (hexDatas[i] = malloc(TX_DATA_CHARS+1)) == NULL) { die("malloc failed"); } }
	if (partialTxidFill) { strcpy(txids[0], partialTxid); strncat(txids[0], treeHexData, partialTxidFill); free(partialTxid); }
	const char *txidPtr = treeHexData+partialTxidFill;
	for (int i=(partialTxidFill ? 1 : 0); i<txidsCount; i++) {
		strncpy(txids[i], txidPtr, TXID_CHARS);
		txids[i][TXID_CHARS] = 0;
		txidPtr += TXID_CHARS;
	}

	int status;
	if ((status = fetchHexData(hexDatas, (const char **)txids, txidsCount)) == CWG_OK) { 
		char hexDataAll[TX_DATA_CHARS*txidsCount + 1];
		hexDataAll[0] = 0;
		for (int i=0; i<txidsCount; i++) { 
			strcat(hexDataAll, hexDatas[i]);
		}

		if (suffixLen != TREE_SUFFIX_CHARS) {
			char *partialTxidN = malloc(TXID_CHARS+1);
			if (partialTxidN == NULL) { die("malloc failed"); }
			partialTxidN[0] = 0; strncat(partialTxidN, txidPtr, numChars-(txidPtr-(treeHexData+partialTxidFill)));
			addFront(partialTxids[1], partialTxidN);
		}

		int subStatus;
		char fileByteData[strlen(hexDataAll)/2];
		int bytesToWrite;
		if ((subStatus = traverseFileTree(hexDataAll, partialTxids, suffixLen == TREE_SUFFIX_CHARS ? suffixLen : 0, fd))
		     == CWG_FETCH_NO) {
			if ((bytesToWrite = hexStrDataToFileBytes(fileByteData, hexDataAll, 1))) { 
				if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) { perror("write() failed");
											    status = CWG_WRITE_ERR;
											    goto cleanup; }
			} else { status = CWG_FILE_ERR; goto cleanup; }

			reverseList(partialTxids[1]);
			struct List *temp = partialTxids[0];
			partialTxids[0] = partialTxids[1];
			partialTxids[1] = temp;
		} else { status = subStatus; }
	}

	cleanup:
		for (int i=0; i<txidsCount; i++) { free(txids[i]); free(hexDatas[i]); }
		return status;
}

static int traverseFileChain(const char *hexDataStart, int fd) {
	char hexData[TX_DATA_CHARS+1];
	strcpy(hexData, hexDataStart);
	char *hexDataNext = malloc(TX_DATA_CHARS+1);
	if (hexDataNext == NULL) { die("malloc failed"); }
	char *txidNext = malloc(TXID_CHARS+1);
	if (txidNext == NULL) { die("malloc failed"); }

	struct List partialTxidsO; struct List partialTxidsN;
	struct List *partialTxids[2] = { &partialTxidsO, &partialTxidsN };
	initList(partialTxids[0]); initList(partialTxids[1]);
	int isTree = 1;

	int status;
	char fileByteData[TX_DATA_BYTES];
	int bytesToWrite;
	int fileEnd=0;
	while (!fileEnd) {
		if (strlen(hexData) >= TXID_CHARS) {
			strcpy(txidNext, hexData+(strlen(hexData) - TXID_CHARS));
			if (fetchHexData(&hexDataNext, (const char **)&txidNext, 1) != CWG_OK) { fileEnd = 1; }	
		} else { fileEnd = 1; }
		if (!isTree ||
		   (status = traverseFileTree(hexData, partialTxids, fileEnd ? TREE_SUFFIX_CHARS : TXID_CHARS, fd)) == CWG_FETCH_NO) {
			if (!(bytesToWrite = hexStrDataToFileBytes(fileByteData, hexData, fileEnd))) { status = CWG_FILE_ERR;
												       goto cleanup; }
			if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) {
				perror("write() failed"); 
				status = CWG_WRITE_ERR; goto cleanup;
			}
			status = CWG_OK;
			isTree = 0;	
		} else if (status != CWG_OK) { goto cleanup; }
		strcpy(hexData, hexDataNext);
	}
	
	cleanup:
		free(txidNext);
		free(hexDataNext);
		return status;
}

int getFile(const char *txid, const char *bdNode, void (*foundHandler) (int, int), int fd) {
	bitdbNode = bdNode;
	if (IS_BITDB_REQUEST_LIMIT) { srandom(time(NULL)); }

	char *hexDataStart = malloc(TX_DATA_CHARS+1);
	int status = fetchHexData(&hexDataStart, (const char **)&txid, 1);
	if (foundHandler != NULL) { foundHandler(status, fd); }

	if (status != CWG_OK) { goto cleanup; }
	status = traverseFileChain(hexDataStart, fd);
	
	cleanup:
		free(hexDataStart);
		return status;
}

int dirPathToTxid(FILE *dirFp, const char *dirPath, char *pathTxid) {
	int status = CWG_DIR_NO;

	struct DynamicMemory line;
	line.size = DIR_LINE_BUF;
	line.data = malloc(line.size);
	bzero(line.data, line.size);

	int lineLen;
	int offset = 0;
	int matched = 0;
	while (fgets(line.data+offset, line.size-1-offset, dirFp) != NULL) {
		lineLen = strlen(line.data);
		if (line.data[lineLen-1] != '\n' && !feof(dirFp)) {
			offset = lineLen;
			line.data = realloc(line.data, (line.size*=2));
			bzero(line.data+offset, line.size-offset);
			continue;
		}
		else if (offset > 0) { offset = 0; line.data[lineLen] = 0; }

		if (strncmp(line.data, dirPath, strlen(dirPath)) == 0) {
			matched = 1;
			fgets(pathTxid, TXID_CHARS+1, dirFp);
			pathTxid[TXID_CHARS] = 0;
			status = CWG_OK;
			break;
		}
	}

	free(line.data);
	return status;
}

int getDirFile(const char *dirTxid, const char *dirPath, const char *bdNode, FILE *writeDirFp, void (*foundHandler) (int, int), int fd) {
	FILE *dirFp;
	if ((dirFp = writeDirFp) == NULL && (dirFp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	char txid[TXID_CHARS+1]; txid[TXID_CHARS] = 0;

	int status;
	if ((status = getFile(dirTxid, bdNode, NULL, fileno(dirFp))) == CWG_OK) {
		rewind(dirFp);
		if ((status = dirPathToTxid(dirFp, dirPath, txid)) == CWG_OK) {
			status = getFile(txid, bdNode, foundHandler, fd);
			foundHandler = NULL;
		}
	} 

	if (foundHandler != NULL) { foundHandler(status, fd); }
	if (writeDirFp == NULL) { fclose(dirFp); } else { rewind(dirFp); }
	return status;
}

