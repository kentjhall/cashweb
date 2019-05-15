#include "cashgettools.h"

/* char *bitdbNode = "https://bitdb.bitcoin.com/q"; */
char *bitdbNode = "https://bitdb.bch.sx/q";

// copies curl response to specified address in memory; needs to be freed
static size_t copyResponseToMemory(void *data, size_t size, size_t nmemb, char **responsePtr) {
	if ((*responsePtr = malloc(nmemb*size + 1)) == NULL) { die("malloc failed"); }
	memcpy(*responsePtr, data, nmemb*size);
	(*responsePtr)[nmemb*size] = 0;
	return nmemb*size;
}

// fetches hex data at specified txid and copies to memory; needs to be freed
static int fetchHexData(char *hexData, const char *txid) {
	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() fails\n"); return 0; }

	int success = 1;

	// construct query
	char query[QUERY_LEN];
	snprintf(query, sizeof(query), "{\"v\":%d,\"q\":{\"find\":{\"tx.h\":\"%s\"}},\"r\":{\"f\":\"[.[0]|{%s:.out[0].h1}]\"}}",
		BITDB_API_VER, txid, QUERY_DATA_TAG);
	char *queryB64;
	if ((queryB64 = b64_encode((const unsigned char *)query, strlen(query))) == NULL) { die("b64 encode failed"); }

	// construct url from query
	char url[strlen(bitdbNode) + strlen(queryB64) + 1 + 1];
	strcpy(url, bitdbNode);
	strcat(url, "/");
	strcat(url, queryB64);
	free(queryB64);

	// send curl request
	char *response = NULL;
	if (IS_BITDB_REQUEST_LIMIT) { // this bit is to trick a server's request limit, although won't necessarily work with every server
		struct curl_slist *headers = NULL;
		char buf[HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d", rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &copyResponseToMemory);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	if ((res = curl_easy_perform(curl)) != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		success = 0;
		goto end;
	} 

	// parse response for hex data
	char *responseParsed;
	if ((responseParsed = strstr(response, RESPONSE_DATA_TAG)) == NULL) { 
		success = 0; 
		goto end; 
	}
	responseParsed += strlen(RESPONSE_DATA_TAG);
	for (int i=0; i<strlen(responseParsed); i++) {
		if (responseParsed[i] == '"') { responseParsed[i] = 0; break; }
	}

	// copy hex data to new memory location
	if (strlen(responseParsed) > TX_DATA_CHARS) { 
		success=0;
		fprintf(stderr, "invalid response, too much data: %s\ntxid is probably incorrect\n", responseParsed); 
		goto end; 
	}
	strcpy(hexData, responseParsed);
	free(response);

	// cleanup and return data
	end:
		curl_easy_cleanup(curl);
		return success;
}

// converts hex bytes to chars, accounting for possible txid at end, and copies to specified memory loc
static int hexStrDataToFileBytes(char *byteData, const char *hexData, int fileEnd) {
	if (!(strlen(hexData) % 2 == 0)) { fprintf(stderr, "invalid hex data"); return 0; }

	memset(byteData, 0, TX_DATA_BYTES+1);
	int tailOmit = !fileEnd ? TXID_CHARS : 0;
	char hexByte[2+1];
	hexByte[2] = 0;
	for (int i=0; i<strlen(hexData)-tailOmit; i+=2) { 
		strncpy(hexByte, hexData+i, 2);
		byteData[i/2] = (char)strtol(hexByte, NULL, 16);
	}
	byteData[(strlen(hexData)-tailOmit)/2] = 0;
	return 1;
}

static int checkIsFileTree(char *hexDataN, const char *hexDataO) {
	char txid[TXID_CHARS+1];
	strncpy(txid, hexDataO, TXID_CHARS);
	txid[TXID_CHARS] = 0;
	return fetchHexData(hexDataN, txid);
}

static void traverseFileChain(const char *hexDataStart, int fd) {
	char hexData[TX_DATA_CHARS+1];
	strcpy(hexData, hexDataStart);
	char hexDataNext[TX_DATA_CHARS+1];
	char txidNext[TXID_CHARS+1];
	char fileByteData[TX_DATA_BYTES+1];
	int fileEnd=0;
	while (!fileEnd) {
		if (strlen(hexData) >= TXID_CHARS) {
			strcpy(txidNext, hexData+(strlen(hexData) - TXID_CHARS));
			if (!fetchHexData(hexDataNext, txidNext)) { fileEnd = 1; }
		} else { fileEnd = 1; }
		if (!hexStrDataToFileBytes(fileByteData, hexData, fileEnd)) { return; }
		if (write(fd, fileByteData, strlen(fileByteData)) < strlen(fileByteData)) { die("write() failed"); }
		strcpy(hexData, hexDataNext);
	}
}

static void traverseFileTree(char *txidRoot, int fd) {
	return; // TODO
}

void getFile(const char *txid, int fd) {
	if (IS_BITDB_REQUEST_LIMIT) { srandom(time(NULL)); }
	char hexData[TX_DATA_CHARS+1];
	if (!fetchHexData(hexData, txid)) { return; }
	traverseFileChain(hexData, fd);	
}
