#ifndef __CASHFETCHUTILS_BITDB_H__
#define __CASHFETCHUTILS_BITDB_H__

#include "cashwebutils.h"
#include <b64/b64.h>

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
#define BITDB_REQUEST_TIMEOUT 20L

static CW_STATUS httpRequest(const char *url, bool reqLimit, FILE *respFp);

/*
 * for writing HTTP response to specified file stream
 * returns number of bytes written
 */
static size_t writeResponseToStream(void *data, size_t size, size_t nmemb, FILE *respStream) {
	return fwrite(data, size, nmemb, respStream)*size;
}

#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
static CW_STATUS httpRequest(const char *url, bool reqLimit, FILE *respFp) {
	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(CWG_err_stream, "curl_easy_init() failed\n"); return CW_SYS_ERR; }	

	struct curl_slist *headers = NULL;
	if (reqLimit) { // this bit is to trick a server's request limit, although won't necessarily work with every server
		char buf[BITDB_HEADER_BUF_SZ];
		snprintf(buf, sizeof(buf), "X-Forwarded-For: %d.%d.%d.%d",
			rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1, rand()%1000 + 1);
		headers = curl_slist_append(headers, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeResponseToStream);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, respFp);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, BITDB_REQUEST_TIMEOUT);
	res = curl_easy_perform(curl);

	if (headers) { curl_slist_free_all(headers); }
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		fprintf(CWG_err_stream, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		return CWG_FETCH_ERR;
	}

	return CW_OK;
}
#else
#include <emscripten.h>
static CW_STATUS httpRequest(const char *url, bool reqLimit, FILE *respFp) {
	void *buffer = NULL;
	int sz;
	int err;
	emscripten_wget_data(url, &buffer, &sz, &err);
	if (!err) {
		if (fwrite(buffer, sz, 1, respFp) < 1) { perror("fwrite() failed"); free(buffer); return CW_SYS_ERR; }
	} else if (err != 1) {
		fprintf(CWG_err_stream, "emscripten_wget_data() failed; unhandled error code %d\n", err);
		if (buffer) { free(buffer); }
		return CWG_FETCH_ERR;
	}
	free(buffer);
	return CW_OK;
}

#define CURL_GLOBAL_DEFAULT
static void curl_global_init() { /* dummy */ }
static void curl_global_cleanup() { /* dummy */ }

#endif

static size_t querySizeExceed;

static inline CW_STATUS fetchSplitHexDataBitDBNode(const char **ids, size_t count, FETCH_TYPE type, const char *bitdbNode, bool bitdbRequestLimit, char **txids, char *hexDataAll);

/*
 * fetches hex data (from BitDB HTTP endpoint) at specified ids and copies (in order) to specified location in memory 
 * id type is specified by FETCH_TYPE type
 * when searching for nametag, count references the nth occurrence to get (as only one nametag can be fetched at a time anyway);
   can be used to skip a nametag claim
 * txids of fetched TXs can be written to txids, or can be set NULL; shouldn't be needed if type is BY_TXID
 */
static CW_STATUS fetchHexDataBitDBNode(const char **ids, size_t count, FETCH_TYPE type, const char *bitdbNode, bool bitdbRequestLimit, char **txids, char *hexDataAll) {
	if (count < 1) { return CWG_FETCH_NO; }

	size_t nth = 1;
	// fetching by nametag does not permit querying for more than one at a time, so count is used for if any occurrences should be skipped
	if (type == BY_NAMETAG) { nth = count; count = 1; } 	

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
					fprintf(CWG_err_stream, "cashgettools: nametag queried is too long\n");
					free(idQuery);
					return CW_CALL_NO;
				}
				printed = snprintf(idQuery+strlen(idQuery), BITDB_ID_QUERY_BUF_SZ, "{\"out.s2\":\"%s\"},", ids[i]);
				break;
			default:
				fprintf(CWG_err_stream, "invalid FETCH_TYPE; problem with cashgettools\n");
				free(idQuery);
				return CW_SYS_ERR;
		}
		if (printed >= BITDB_ID_QUERY_BUF_SZ) {
			fprintf(CWG_err_stream, "BITDB_ID_QUERY_BUF_SZ set too small; problem with cashgettools\n");
			free(idQuery);
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
			fprintf(CWG_err_stream, "invalid FETCH_TYPE; problem with cashgettools\n");
			free(idQuery);
			return CW_SYS_ERR;
	}
	if (printed >= sizeof(respHandler)) {
		fprintf(CWG_err_stream, "BITDB_RESPHANDLE_QUERY_BUF_SZ set too small; problem with cashgettools\n");
		free(idQuery);
		return CW_SYS_ERR;
	}

	char specifiersStr[] = ",\"sort\":{\"blk.i\":1,\"tx.h\":1},\"limit\":1,\"skip\":";
	char specifiers[sizeof(specifiersStr) + 15]; specifiers[0] = 0;
	if (type == BY_NAMETAG) { snprintf(specifiers, sizeof(specifiers), "%s%zu", specifiersStr, nth-1); }

	char query[BITDB_QUERY_BUF_SZ + strlen(specifiersStr) + strlen(idQuery) + strlen(respHandler) + 1];
	printed = snprintf(query, sizeof(query), 
		  "{\"v\":%d,\"q\":{\"find\":{\"$or\":[%s]}%s},\"r\":{\"f\":\"[.[]|%s]\"}}",
	    	  BITDB_API_VER, idQuery, specifiers, respHandler);
	free(idQuery);
	if (printed >= sizeof(query)) {
		fprintf(CWG_err_stream, "BITDB_QUERY_BUF_SZ set too small; problem with cashgettools\n");
		return CW_SYS_ERR;
	}
	size_t queryLen = strlen(query);
	if (querySizeExceed && queryLen >= querySizeExceed) { return fetchSplitHexDataBitDBNode(ids, count, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll); }

	char *queryB64;
	if ((queryB64 = b64_encode((const unsigned char *)query, queryLen)) == NULL) { perror("b64 encode failed"); return CW_SYS_ERR; }
	char url[strlen(bitdbNode) + strlen(queryB64) + 1 + 1];

	// construct url from query
	strcpy(url, bitdbNode);
	strcat(url, "/");
	strcat(url, queryB64);
	free(queryB64);	

	// initializing variable-length arrays before goto statements
	const char *hexDataPtrs[count];
	bool added[count];

	CW_STATUS status;

	// send request
	FILE *respFp;
	if ((respFp = tmpfile()) == NULL) { perror("tmpfile() failed"); return CW_SYS_ERR; }
	if ((status = httpRequest(url, bitdbRequestLimit, respFp)) != CW_OK) {
		fclose(respFp);
		return status;
	}
	rewind(respFp);
	
	// load response json from file and handle potential errors
	json_error_t jsonError;
	json_t *respJson = json_loadf(respFp, 0, &jsonError);
	if (respJson == NULL) {
		fseek(respFp, 0, SEEK_END);
		long respSz = ftell(respFp);
		fseek(respFp, 0, SEEK_SET);
		char respMsg[respSz+1];
		respMsg[respSz > 0 ? fread(respMsg, 1, respSz, respFp) : 0] = 0;
		if (count > 1 && (strlen(respMsg) < 1 || (strstr(respMsg, "URI") && strstr(respMsg, "414")))) { // catch for Request-URI Too Large or empty response body
			querySizeExceed = queryLen;
			status = fetchSplitHexDataBitDBNode(ids, count, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll);
			goto cleanup;
		}
		else if (strstr(respMsg, "html")) {
			fprintf(CWG_err_stream, "HTML response error unhandled in cashgettools:\n%s\n", respMsg);
			status = CWG_FETCH_ERR;
			goto cleanup;
		}
		else {
			fprintf(CWG_err_stream, "jansson failed to parse response from BitDB node: %s\nResponse:\n%s\n", jsonError.text, respMsg);
			status = CWG_FETCH_ERR;
			goto cleanup;
		}
	}

	const char *errMsg;
	if ((errMsg = json_string_value(json_object_get(respJson, "error")))) {
		if (strstr(errMsg, "requests")) {
			sleep(1);
			status = fetchHexDataBitDBNode(ids, count, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll);
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
				fprintf(CWG_err_stream, "BitDB node responded with unexpected JSON format:\n%s\n", jsonDump);
				free(jsonDump);
				status = CWG_FETCH_ERR;
				goto cleanup;
			}
			
			json_array_foreach(jsonArrs[a], index, dataJson) {
				if ((dataId = json_string_value(json_object_get(dataJson, BITDB_QUERY_ID_TAG))) == NULL ||
				    (dataHex = json_string_value(json_object_get(dataJson, BITDB_QUERY_DATA_TAG))) == NULL) {
				    	if (dataId && json_is_null(json_object_get(dataJson, BITDB_QUERY_DATA_TAG))) { continue; }
				    	jsonDump = json_dumps(jsonArrs[a], 0);
					fprintf(CWG_err_stream, "BitDB node responded with unexpected JSON format:\n%s\n", jsonDump);
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
							fprintf(CWG_err_stream, "BitDB node responded with unexpected JSON format:\n%s\n", jsonDump);
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
		if (respFp) { fclose(respFp); }
		return status;
}

static inline CW_STATUS fetchSplitHexDataBitDBNode(const char **ids, size_t count, FETCH_TYPE type, const char *bitdbNode, bool bitdbRequestLimit, char **txids, char *hexDataAll) {
	size_t firstCount = count/2;
	CW_STATUS status1 = fetchHexDataBitDBNode(ids, firstCount, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll);
	CW_STATUS status2 = fetchHexDataBitDBNode(ids+firstCount, count-firstCount, type, bitdbNode, bitdbRequestLimit, txids, hexDataAll+strlen(hexDataAll));
	return status1 > status2 ? status1 : status2;
}

#endif
