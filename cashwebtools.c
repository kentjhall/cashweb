#include "cashwebtools.h"

char *bitdbNode = "https://bitdb.bitcoin.com/q";

static void die(char *e) { perror(e); exit(1); }

static size_t writeResponseToMemory(void *data, size_t size, size_t nmemb, char **responsePtr) {
	if ((*responsePtr = malloc(nmemb*size + 1)) == NULL) { die("malloc failed"); }
	memcpy(*responsePtr, data, nmemb*size);
	(*responsePtr)[nmemb*size] = 0;
	return nmemb*size;
}

static char *fetchHexData(char *txid) {
	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(stderr, "curl_easy_init() fails\n"); return NULL; }

	char *hexData = NULL;

	// construct query
	char query[QUERY_LEN];
	snprintf(query, sizeof(query), "{\"v\":%d,\"q\":{\"find\":{\"tx.h\":\"%s\"}},\"r\":{\"f\":\"[.[0]|{data:.out[0].h1}]\"}}",
		BITDB_API_VER, txid);
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
	if (IS_BITDB_REQUEST_LIMIT) {
		struct curl_slist *headers = NULL;
		char buf[HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d", rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeResponseToMemory);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	/* do { */
	if ((res = curl_easy_perform(curl)) != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		goto end;
	} 

	// parse response for hex data
	char *responseParsed = response;
	char *responseHead;
	if (responseParsed != NULL && strlen(responseParsed) > strlen(EXPECTED_RESPONSE_HEAD_C)) {
		if (strncmp(responseParsed, EXPECTED_RESPONSE_HEAD_C, strlen(EXPECTED_RESPONSE_HEAD_C)) == 0) {
			responseHead = EXPECTED_RESPONSE_HEAD_C;
		} else if (strncmp(responseParsed, EXPECTED_RESPONSE_HEAD_U, strlen(EXPECTED_RESPONSE_HEAD_U)) == 0) {
			responseHead = EXPECTED_RESPONSE_HEAD_U;
		} else { fprintf(stderr, "received invalid response: %s\n", responseParsed); goto end; }
	} else { goto end; }
	responseParsed += strlen(responseHead);
	int len = strlen(responseParsed);
	for (int i=strlen(responseHead); i<len; i++) {
		if (responseParsed[i] == '"') { responseParsed[i] = 0; break; }
	}

	// copy hex data to new memory location
	hexData = malloc(strlen(responseParsed)+1);
	strcpy(hexData, responseParsed);
	free(response);

	// cleanup and return data
	end:
		curl_easy_cleanup(curl);
		if (hexData == NULL) { printf("\n\n%s\n\n", txid); }
		return hexData;
}

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

void fetchFileWrite(char *txidStart, int fd) {
	if (strlen(txidStart) != TXID_CHARS) { fprintf(stderr, "invalid txid"); return; }
	char *hexData;
	char *hexDataNext;
	char txid[TXID_CHARS+1];
	char txidNext[TXID_CHARS+1];
	char *fileByteData;
	int fileEnd=0;
	strcpy(txid, txidStart);
	if ((hexData = fetchHexData(txid)) == NULL) { fprintf(stderr, "fetch failed; txid is probably incorrect\n"); return; }
	do {
		if (strlen(hexData) >= TXID_CHARS) {
			strcpy(txidNext, hexData+(strlen(hexData) - TXID_CHARS));
			if ((hexDataNext = fetchHexData(txidNext)) == NULL) { fileEnd = 1; }
		} else { fileEnd = 1; }
		fileByteData = hexStrDataToFileBytes(hexData, fileEnd);
		if (write(fd, fileByteData, strlen(fileByteData)) < strlen(fileByteData)) { die("write() failed"); }
		free(fileByteData);
		strcpy(txid, txidNext); 
		hexData = hexDataNext;
	} while (!fileEnd);
}
