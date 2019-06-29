#include "cashgettools.h"

static const char *bitdbNode;

struct DynamicMemory {
	char *data;
	size_t size;
};

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
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() fails\n"); return 0; }

	int success = 1;

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
		success = 0;
		goto end;
	} 
	(responseDm.data)[responseDm.size] = 0;
	response = responseDm.data;
	if (strstr(response, "414 ")) { // catch for Request-URI Too Large
		int firstCount = count/2;
		return fetchHexData(hexDatas, txids, firstCount) && fetchHexData(hexDatas, txids+firstCount, count-firstCount);
	}

	char *dataTxidPtr;
	char dataTxid[TXID_CHARS+1];
	int pos = 0;
	for (int i=0; i<count; i++) {
		// parse each response for hex data
		if ((responsesParsed[i] = strstr(response+pos, RESPONSE_DATA_TAG)) == NULL) { 
			success = 0; 
			goto end;
		}
		responsesParsed[i] += strlen(RESPONSE_DATA_TAG);
		pos = (responsesParsed[i] - response);
		for (int j=0; j<strlen(responsesParsed[i]); j++) {
			if (responsesParsed[i][j] == '"') { responsesParsed[i][j] = 0; pos += j+1; break; }
		}

		// copy each hex data to memory location by corresponding txid
		if ((dataTxidPtr = strstr(response+pos, RESPONSE_TXID_TAG)) == NULL) { success = 0; goto end; }
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

		if (!matched) { success = 0; goto end; }
	}

	// cleanup and return true/false 
	end:
		if (response != NULL) { free(response); }
		curl_easy_cleanup(curl);
		return success;
}

// converts hex bytes to chars, accounting for possible txid at end, and copies to specified memory loc
// returns number of bytes to write
static int hexStrDataToFileBytes(char *byteData, const char *hexData, int fileEnd) {
	if (!(strlen(hexData) % 2 == 0)) { fprintf(stderr, "invalid hex data\n"); return 0; }

	int tailOmit = !fileEnd ? TXID_CHARS : 0;
	char hexByte[2+1];
	hexByte[2] = 0;
	for (int i=0; i<strlen(hexData)-tailOmit; i+=2) { 
		strncpy(hexByte, hexData+i, 2);
		byteData[i/2] = (char)strtoul(hexByte, NULL, 16);
	}
	return (int)(strlen(hexData)-tailOmit)/2;
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

	int fetched = 0;
	if (fetchHexData(hexDatas, (const char **)txids, txidsCount)) { 
		fetched = 1;

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

		char fileByteData[strlen(hexDataAll)/2];
		int bytesToWrite;
		if (!traverseFileTree(hexDataAll, partialTxids, suffixLen == TREE_SUFFIX_CHARS ? suffixLen : 0, fd)) {
			if ((bytesToWrite = hexStrDataToFileBytes(fileByteData, hexDataAll, 1))) { 
				if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) { perror("write() failed"); }
			}

			reverseList(partialTxids[1]);
			struct List *temp = partialTxids[0];
			partialTxids[0] = partialTxids[1];
			partialTxids[1] = temp;
		}
	}

	for (int i=0; i<txidsCount; i++) { free(txids[i]); free(hexDatas[i]); }
	return fetched;
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
	
	/* char **treeTxids = NULL; */
	/* char **treeHexDatas = NULL; */
	/* int treeTxidsCount[2] = {0}; */
	int isTree = 1;

	char fileByteData[TX_DATA_BYTES];
	int bytesToWrite;
	int fileEnd=0;
	while (!fileEnd) {
		if (strlen(hexData) >= TXID_CHARS) {
			strcpy(txidNext, hexData+(strlen(hexData) - TXID_CHARS));
			if (!fetchHexData(&hexDataNext, (const char **)&txidNext, 1)) { fileEnd = 1; }	
		} else { fileEnd = 1; }
		if (!isTree || !traverseFileTree(hexData, partialTxids, fileEnd ? TREE_SUFFIX_CHARS : TXID_CHARS, fd)) {
			if (!(bytesToWrite = hexStrDataToFileBytes(fileByteData, hexData, fileEnd))) { return 0; }
			if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) { perror("write() failed"); return 0; }
			isTree = 0;	
		}
		strcpy(hexData, hexDataNext);
	}
	free(txidNext);
	free(hexDataNext);
	return 1;
}

int getFile(const char *txid, const char *bdNode, int fd, void (*foundHandler) (int, int)) {
	bitdbNode = bdNode;
	if (IS_BITDB_REQUEST_LIMIT) { srandom(time(NULL)); }
	char *hexDataStart = malloc(TX_DATA_CHARS+1);
	int success = fetchHexData(&hexDataStart, (const char **)&txid, 1);
	if (foundHandler != NULL) { foundHandler(success, fd); }
	if (!success) { goto end; }
	success = traverseFileChain(hexDataStart, fd);

	end:
		free(hexDataStart);
		return success;
}
