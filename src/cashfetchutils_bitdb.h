#ifndef __CASHFETCHUTILS_BITDB_H__
#define __CASHFETCHUTILS_BITDB_H__

#include "cashwebutils.h"
#include <b64/b64.h>
#include <curl/curl.h>

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

/*
 * for writing curl response to specified file stream
 */
static size_t writeResponseToStream(void *data, size_t size, size_t nmemb, FILE *respStream) {
	return fwrite(data, size, nmemb, respStream)*size;
}

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

	CURL *curl;
	CURLcode res;
	if (!(curl = curl_easy_init())) { fprintf(CWG_err_stream, "curl_easy_init() failed\n"); return CWG_FETCH_ERR; }

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
					curl_easy_cleanup(curl);
					return CW_CALL_NO;
				}
				printed = snprintf(idQuery+strlen(idQuery), BITDB_ID_QUERY_BUF_SZ, "{\"out.s2\":\"%s\"},", ids[i]);
				break;
			default:
				fprintf(CWG_err_stream, "invalid FETCH_TYPE; problem with cashgettools\n");
				free(idQuery);
				curl_easy_cleanup(curl);
				return CW_SYS_ERR;
		}
		if (printed >= BITDB_ID_QUERY_BUF_SZ) {
			fprintf(CWG_err_stream, "BITDB_ID_QUERY_BUF_SZ set too small; problem with cashgettools\n");
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
			fprintf(CWG_err_stream, "invalid FETCH_TYPE; problem with cashgettools\n");
			free(idQuery);
			curl_easy_cleanup(curl);
			return CW_SYS_ERR;
	}
	if (printed >= sizeof(respHandler)) {
		fprintf(CWG_err_stream, "BITDB_RESPHANDLE_QUERY_BUF_SZ set too small; problem with cashgettools\n");
		free(idQuery);
		curl_easy_cleanup(curl);
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
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, BITDB_REQUEST_TIMEOUT);
	res = curl_easy_perform(curl);
	if (headers) { curl_slist_free_all(headers); }
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) {
		fprintf(CWG_err_stream, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
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
			fprintf(CWG_err_stream, "HTML response error unhandled in cashgettools:\n%s\n", respMsg);
			status = CWG_FETCH_ERR;
			goto cleanup;
		}
		else {
			fprintf(CWG_err_stream, "jansson error in parsing response from BitDB node: %s\nResponse:\n%s\n", jsonError.text, respMsg);
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
		fclose(respFp);
		return status;
}

#endif
