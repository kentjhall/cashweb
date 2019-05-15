#include "cashgettools.h"

char *bitdbNode = "https://bitdb.bitcoin.com/q";

// copies curl response to specified address in memory; needs to be freed
static size_t copyResponseToMemory(void *data, size_t size, size_t nmemb, char **responsePtr) {
	if ((*responsePtr = malloc(nmemb*size + 1)) == NULL) { die("malloc failed"); }
	memcpy(*responsePtr, data, nmemb*size);
	(*responsePtr)[nmemb*size] = 0;
	return nmemb*size;
}

// fetches hex data at specified txid and copies to memory; needs to be freed
static char *fetchHexData(char *txid) {
	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() fails\n"); return NULL; }

	char *hexData = NULL;

	// construct query
	char query[QUERY_LEN];
	snprintf(query, sizeof(query), "{\"v\":%d,\"q\":{\"find\":{\"tx.h\":\"%s\"}},\"r\":{\"f\":\"[.[0]|{%s:.out[0].h1}]\"}}",
		BITDB_API_VER, txid, RESPONSE_DATA_TAG);
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
		goto end;
	} 

	// parse response for hex data
	char *responseParsed;
	if ((responseParsed = strstr(response, RESPONSE_DATA_TAG_QUERY)) == NULL) { goto end; }
	responseParsed += strlen(RESPONSE_DATA_TAG_QUERY);
	for (int i=0; i<strlen(responseParsed); i++) {
		if (responseParsed[i] == '"') { responseParsed[i] = 0; break; }
	}

	// copy hex data to new memory location
	hexData = malloc(strlen(responseParsed)+1);
	strcpy(hexData, responseParsed);
	free(response);

	// cleanup and return data
	end:
		curl_easy_cleanup(curl);
		return hexData;
}

// converts hex bytes to chars, accounting for possible txid at end, and copies to memory; needs to be freed
static char *hexStrDataToFileBytes(char *hexData, int fileEnd) {
	if (!(strlen(hexData) % 2 == 0)) { fprintf(stderr, "invalid hex data"); return NULL; }

	int tailOmit = !fileEnd ? TXID_CHARS : 0;
	char *byteData;
	if ((byteData = malloc(((strlen(hexData)-tailOmit)/2)+1)) == NULL) { die("malloc failed"); }
	char hexByte[2+1];
	hexByte[2] = 0;
	for (int i=0; i<strlen(hexData)-tailOmit; i+=2) { 
		strncpy(hexByte, hexData+i, 2);
		byteData[i/2] = (char)strtol(hexByte, NULL, 16);
	}
	byteData[(strlen(hexData)-tailOmit)/2] = 0;
	free(hexData);
	return byteData;
}

static void getWriteFileChain(char *txidStart, int fd) {
	if (strlen(txidStart) != TXID_CHARS) { fprintf(stderr, "invalid txid\n"); return; }
	char *hexData;
	char *hexDataNext;
	char txid[TXID_CHARS+1];
	char txidNext[TXID_CHARS+1];
	char *fileByteData;
	int fileEnd=0;
	strcpy(txid, txidStart);
	if ((hexData = fetchHexData(txid)) == NULL) { fprintf(stderr, "fetch failed; txid is probably incorrect\n"); return; }
	while (!fileEnd) {
		if (strlen(hexData) >= TXID_CHARS) {
			strcpy(txidNext, hexData+(strlen(hexData) - TXID_CHARS));
			if ((hexDataNext = fetchHexData(txidNext)) == NULL) { fileEnd = 1; }
		} else { fileEnd = 1; }
		fileByteData = hexStrDataToFileBytes(hexData, fileEnd);
		if (write(fd, fileByteData, strlen(fileByteData)) < strlen(fileByteData)) { die("write() failed"); }
		free(fileByteData);
		strcpy(txid, txidNext); 
		hexData = hexDataNext;
	}
}

static void getWriteFileTree(char *txidRoot, int fd) {
	return; // TODO
}

void getWriteFile(char *txid, int fd) {
	if (IS_BITDB_REQUEST_LIMIT) { srandom(time(NULL)); }
	getWriteFileChain(txid, fd);	
}
