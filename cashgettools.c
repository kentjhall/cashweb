#include "cashgettools.h"

char *bitdbNode = "https://bitdb.bitcoin.com/q";

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
	memset(added, 0, count);

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

static int traverseFileTree(const char *treeHexData, int fd, char ***firstHexDatas, char ***prevTxids, int *prevTxidsCount, int isChained) {
	// defaults for when not segment in chain (full tree), so these can be set to NULL
	char **nullPtr = NULL;
	int zeros[2] = {0};
	if (firstHexDatas == NULL) { firstHexDatas = &nullPtr; }
	if (prevTxids == NULL) { prevTxids = &nullPtr; }
	if (prevTxidsCount == NULL) { prevTxidsCount = zeros; }

	// check for partial txid in last segment of chain; if so, fill it with beginning of this segment
	int partialTxidFill = 0;
	if (*prevTxids != NULL && *prevTxidsCount > 0 && (partialTxidFill = TXID_CHARS-strlen((*prevTxids)[*prevTxidsCount-1])) > 0) {
		strncat((*prevTxids)[*prevTxidsCount-1], treeHexData, partialTxidFill);
	}

	// calculate number of txids, excluding last bytes dependent on whether this is segment in chain
	int numChars = strlen(treeHexData+partialTxidFill)-(isChained ? TXID_CHARS : TREE_SUFFIX_LEN);
	int txidsCount = *prevTxidsCount + (int)ceil((double)numChars/TXID_CHARS);

	// if this is not a segment in a chain (alleged full tree), check that it is a tree by proper number of characters
	if (!isChained && *prevTxids == NULL && numChars%TXID_CHARS != 0) { return 0; } 

	// allocate space for txids, and then copy from tree data
	char **txids = *prevTxids = realloc(*prevTxids, txidsCount*sizeof(char *));
	if (txids == NULL) { die("malloc failed"); }
	char treeHexDataTxids[numChars];
	strncpy(treeHexDataTxids, treeHexData+partialTxidFill, numChars);
	treeHexDataTxids[numChars] = 0;
	for (int i=*prevTxidsCount; i<txidsCount; i++) {
		if ((txids[i] = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
		strncpy(txids[i], treeHexDataTxids+((i-*prevTxidsCount)*TXID_CHARS), TXID_CHARS);
		txids[i][TXID_CHARS] = 0;
	}
	*prevTxidsCount = txidsCount;

	// fetch/save first hex datas to check that this is a file tree
	if (*firstHexDatas == NULL && isChained) { 
		int firstTxidsCount = txidsCount-(strlen(txids[txidsCount-1]) < TXID_CHARS ? 1 : 0);
		if ((*firstHexDatas = malloc(firstTxidsCount*sizeof(char *))) == NULL) { die("malloc failed"); }
		for (int i=0; i<firstTxidsCount; i++) {
			if (((*firstHexDatas)[i] = malloc(TX_DATA_CHARS+1)) == NULL) { die("malloc failed"); }
		}
		if (!fetchHexData(*firstHexDatas, (const char **)txids, firstTxidsCount)) {
			for (int i=0; i<txidsCount; i++) {
				free(txids[i]);
				if (i<firstTxidsCount) { free((*firstHexDatas)[i]); }
			}
			free(txids); free(*firstHexDatas);
			return 0;
		}
		prevTxidsCount[1] = firstTxidsCount; // second value in prevTxidsCount is for storing number of txids already fetched in firstHexDatas
	}

	// if there is no next chain segment, fetch remaining hex datas (since first) and write to fd
	if (!isChained) {
		char **hexDatas = realloc(*firstHexDatas, txidsCount*sizeof(char *));
		if (hexDatas == NULL) { die("malloc failed"); }
		int firstTxidsCount = prevTxidsCount[1];
		for (int i=firstTxidsCount; i<txidsCount; i++) {
			if ((hexDatas[i] = malloc(TX_DATA_CHARS+1)) == NULL) { die("malloc failed"); }
		}	
		if (!fetchHexData(hexDatas+firstTxidsCount, (const char **)txids+firstTxidsCount, txidsCount-firstTxidsCount)) {
			if (prevTxidsCount == zeros) { fprintf(stderr, "%d\n", firstTxidsCount); }	
			for (int i=0; i<txidsCount; i++) { free(hexDatas[i]); free(txids[i]); }
			free(hexDatas); free(txids);
			return 0;
		}

		char hexDataAll[TX_DATA_CHARS*txidsCount + 1];
		hexDataAll[0] = 0;
		for (int i=0; i<txidsCount; i++) { 
			strcat(hexDataAll, hexDatas[i]);
			free(hexDatas[i]); free(txids[i]);
		}
		free(hexDatas);
		free(txids);

		char fileByteData[strlen(hexDataAll)/2];
		int bytesToWrite;
		if (!traverseFileTree(hexDataAll, fd, NULL, NULL, NULL, 0)) {
			if ((bytesToWrite = hexStrDataToFileBytes(fileByteData, hexDataAll, 1))) { 
				if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) { die("write() failed"); }
			}
		}
	}

	return 1;
}

static int traverseFileChain(const char *hexDataStart, int fd) {
	char hexData[TX_DATA_CHARS+1];
	strcpy(hexData, hexDataStart);
	char *hexDataNext = malloc(TX_DATA_CHARS+1);
	if (hexDataNext == NULL) { die("malloc failed"); }
	char *txidNext = malloc(TXID_CHARS+1);
	if (txidNext == NULL) { die("malloc failed"); }

	char **treeTxids = NULL;
	char **treeHexDatas = NULL;
	int treeTxidsCount[2] = {0};
	int isTree = 1;

	char fileByteData[TX_DATA_BYTES];
	int bytesToWrite;
	int fileEnd=0;
	while (!fileEnd) {
		if (strlen(hexData) >= TXID_CHARS) {
			strcpy(txidNext, hexData+(strlen(hexData) - TXID_CHARS));
			if (!fetchHexData(&hexDataNext, (const char **)&txidNext, 1)) { fileEnd = 1; }	
		} else { fileEnd = 1; }
		if (!isTree || !traverseFileTree(hexData, fd, &treeHexDatas, &treeTxids, treeTxidsCount, !fileEnd)) {
			if (!(bytesToWrite = hexStrDataToFileBytes(fileByteData, hexData, fileEnd))) { return 0; }
			if (write(fd, fileByteData, bytesToWrite) < bytesToWrite) { die("write() failed"); }
			isTree = 0;	
		}
		strcpy(hexData, hexDataNext);
	}
	free(txidNext);
	free(hexDataNext);
	return 1;
}

int getFile(const char *txid, int fd) {
	if (IS_BITDB_REQUEST_LIMIT) { srandom(time(NULL)); }
	char *hexDataStart = malloc(TX_DATA_CHARS+1);
	if (!fetchHexData(&hexDataStart, (const char **)&txid, 1)) { return 0; }
	if (!traverseFileChain(hexDataStart, fd)) { return 0; }
	free(hexDataStart);
	return 1;
}
