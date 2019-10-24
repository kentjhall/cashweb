#include "cashfetchutils.h"
#include "cashfetchhttputils.h"
#include <mongoc.h>

/* MongoDB constants */
#define MONGODB_APPNAME "cashgettools"

/*
 * fetches hex data (from MongoDB populated by BitDB) at specified ids and copies (in order) to specified location in memory 
 * id type is specified by FETCH_TYPE type
 * when searching for nametag, count references the nth occurrence to get (as only one nametag can be fetched at a time anyway);
   can be used to skip a nametag claim
 * txids of fetched TXs can be written to txids, or can be set NULL; shouldn't be needed if type is BY_TXID
 */
static CW_STATUS fetchHexDataMongoDB(const char **ids, size_t count, FETCH_TYPE type, mongoc_client_t *mongodbCli, char **txids, char *hexDataAll) {
	if (count < 1) { return CWG_FETCH_NO; }

	size_t nth = 1;
	if (type == BY_NAMETAG) { nth = count; count = 1; }

	CW_STATUS status = CW_OK;
	
	hexDataAll[0] = 0;
	mongoc_collection_t *colls[2] = { mongoc_client_get_collection(mongodbCli, "bitdb", "confirmed"), 
					  mongoc_client_get_collection(mongodbCli, "bitdb", "unconfirmed") };
	bson_t *query = NULL;
	bson_t *opts = NULL;
	switch (type) {
		case BY_TXID:
			opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "_id", BCON_BOOL(false), "}");
			break;
		case BY_INTXID:
			opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "in", BCON_BOOL(true), "tx", BCON_BOOL(true), "_id", BCON_BOOL(false), "}");
			break;
		case BY_NAMETAG:
			opts = BCON_NEW("projection", "{", "out", BCON_BOOL(true), "tx", BCON_BOOL(true), "_id", BCON_BOOL(false), "}",
					"sort", "{", "blk.i", BCON_INT32(1), "tx.h", BCON_INT32(1), "}",
					"limit", BCON_INT64(1),
					"skip", BCON_INT64(nth-1));
			break;
		default:
			fprintf(CWG_err_stream, "invalid FETCH_TYPE; problem with cashgettools\n");
			goto cleanup;
	}	

	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *res;
	char *resStr;
	json_t *resJson;
	char  *jsonDump;
	json_error_t jsonError;
	char hexData[CW_TX_DATA_CHARS+1];
	char *token;
	const char *str;
	const char *txid;
	const char *inTxid;
	int vout;
	size_t hexPrefixLen = strlen(DATA_STR_PREFIX);
	bool matched;
	for (int i=0; i<count; i++) { 
		matched = false;
		switch (type) {
			case BY_TXID:
				query = BCON_NEW("tx.h", ids[i]);
				break;
			case BY_INTXID:
				query = BCON_NEW("in.e.h", ids[i]);
				break;
			case BY_NAMETAG:
				query = BCON_NEW("out.s2", ids[i]);
				break;
		}
		for (int c=0; c<sizeof(colls)/sizeof(colls[0]); c++) {
			cursor = mongoc_collection_find_with_opts(colls[c], query, opts, NULL);
			while (mongoc_cursor_next(cursor, &res)) { 
				resStr = bson_as_relaxed_extended_json(res, NULL);
				resJson = json_loads(resStr, JSON_ALLOW_NUL, &jsonError);
				bson_free(resStr);
				if (resJson == NULL) {
					fprintf(CWG_err_stream, "jansson error in parsing result from MongoDB query: %s\nResponse:\n%s\n", jsonError.text, resStr);
					status = CW_SYS_ERR;
					break;
				}

				if (type == BY_INTXID) {
					// gets json array at key 'out' -> object at array index 0 -> object at key 'e' -> object at key 'i' (.in[0].e.i)
					vout = json_integer_value(json_object_get(json_object_get(json_array_get(json_object_get(resJson, "in"), 0), "e"), "i"));
					inTxid = json_string_value(json_object_get(json_object_get(json_array_get(json_object_get(resJson, "in"), 0), "e"), "h"));
					if (!inTxid) {
						jsonDump = json_dumps(resJson, 0);
						json_decref(resJson);
						fprintf(CWG_err_stream, "invalid response from MongoDB:\n%s\n", jsonDump);
						free(jsonDump);
						status = CWG_FETCH_ERR;
						break;
					}
					if (vout != CW_REVISION_INPUT_VOUT || strcmp(inTxid, ids[i]) != 0) { json_decref(resJson); continue; }
				}

				// gets json array at key 'out' -> object at array index 0 -> object at key 'str' (.out[0].str)
				str = json_string_value(json_object_get(json_array_get(json_object_get(resJson, "out"), 0), "str"));	
				if (!str) {
					jsonDump = json_dumps(resJson, 0);
					json_decref(resJson);
					fprintf(CWG_err_stream, "invalid response from MongoDB:\n%s\n", jsonDump);
					free(jsonDump);
					status = CWG_FETCH_ERR;
					break;
				} 
				if (strncmp(str, DATA_STR_PREFIX, hexPrefixLen) != 0) { status = CWG_FILE_ERR; json_decref(resJson); break; }

				hexData[0] = 0; strncat(hexData, str+hexPrefixLen, CW_TX_DATA_CHARS);
				if ((token = strchr(hexData, ' '))) { *token = 0; }

				strncat(hexDataAll, hexData, CW_TX_DATA_CHARS);
				if (txids) {
					if (type == BY_TXID) { txid = ids[i]; }
					else { txid = json_string_value(json_object_get(json_object_get(resJson, "tx"), "h")); }	
					if (!txid) {
						jsonDump = json_dumps(resJson, 0);
						json_decref(resJson);
						fprintf(CWG_err_stream, "invalid response from MongoDB:\n%s\n", jsonDump);
						free(jsonDump);
						status = CWG_FETCH_ERR;
						break;
					}
					txids[i][0] = 0; strncat(txids[i], txid, CW_TXID_CHARS);
				}
				json_decref(resJson);
				matched = true;
				break;
			}
			if (mongoc_cursor_error(cursor, &error)) {
				fprintf(CWG_err_stream, "ERROR: MongoDB query failed\nMessage: %s\n", error.message);
				status = CWG_FETCH_ERR;
			} 
			mongoc_cursor_destroy(cursor);
			if (status != CW_OK) { break; }
		}
		bson_destroy(query);
		if (!matched) { status = status == CW_OK ? CWG_FETCH_NO : status; break; }
	}

	cleanup:
		if (opts) { bson_destroy(opts); }
		for (int c=0; c<sizeof(colls)/sizeof(colls[0]); c++) { mongoc_collection_destroy(colls[c]); }
		return status;
}

/*
 * fetched hex data(s) at specified id(s) of specified type; fetch source is determined by params
 * writes txids (in order) to provided pointer (if not NULL), and writes all hex data (in order) to hexDataAll
 */
CW_STATUS fetchHexData(const char **ids, size_t count, FETCH_TYPE type, struct CWG_params *params, char **txids, char *hexDataAll) {
	if (params->mongodbCli) { return fetchHexDataMongoDB(ids, count, type, (mongoc_client_t *)params->mongodbCli, txids, hexDataAll); }
	else if (params->bitdbNode) { return fetchHexDataBitDBNode(ids, count, type, params->bitdbNode, params->requestLimit, txids, hexDataAll); }
	else if (params->restEndpoint) { return fetchHexDataREST(ids, count, type, params->restEndpoint, params->requestLimit, txids, hexDataAll); }
	else {
		fprintf(CWG_err_stream, "ERROR: neither MongoDB nor BitDB HTTP endpoint address is set in cashgettools implementation\n");
		return CW_CALL_NO;
	}
}

/*
 * initializes for fetcher depending on params
 * should only be called from public functions that will get
 */
CW_STATUS initFetcher(struct CWG_params *params) {
	if (params->mongodb || params->mongodbCli || params->mongodbCliPool) {
		if (params->mongodbCliPool) {
			params->mongodbCli = mongoc_client_pool_pop((mongoc_client_pool_t *)params->mongodbCliPool);
		}
		else if (!params->mongodbCli) { 
			mongoc_init();
			bson_error_t error;	
			mongoc_uri_t *uri;
			if (!(uri = mongoc_uri_new_with_error(params->mongodb, &error))) {
				fprintf(CWG_err_stream, "ERROR: cashgettools failed to parse provided MongoDB URI: %s\nMessage: %s\n", params->mongodb, error.message);
				mongoc_cleanup();
				return CW_CALL_NO;
			}
			params->mongodbCli = (void *)mongoc_client_new_from_uri(uri);	
			mongoc_uri_destroy(uri);	
			if (!params->mongodbCli) {
				fprintf(CWG_err_stream, "ERROR: cashgettools failed to establish client with MongoDB\n");
				mongoc_cleanup();
				return CWG_FETCH_ERR;
			}
			mongoc_client_set_error_api((mongoc_client_t *)params->mongodbCli, MONGOC_ERROR_API_VERSION_2);
			mongoc_client_set_appname((mongoc_client_t *)params->mongodbCli, MONGODB_APPNAME);
		}	
	} 
	else if (params->bitdbNode || params->restEndpoint) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		if (params->requestLimit) { srandom(time(NULL)); }
	}	
	else {
		fprintf(CWG_err_stream, "ERROR: cashgettools requires either MongoDB or BitDB Node address to be specified\n");
		return CW_CALL_NO;
	}

	return CW_OK;
}

/*
 * cleans up for fetcher depending on params
 * should only be called from public functions that have called initFetcher()
 */
void cleanupFetcher(struct CWG_params *params) {
	if (params->mongodbCli) {
		if (params->mongodbCliPool) {
			mongoc_client_pool_push((mongoc_client_pool_t *)params->mongodbCliPool, (mongoc_client_t *)params->mongodbCli);
		}
		else if (params->mongodb) {
			mongoc_client_destroy((mongoc_client_t *)params->mongodbCli);
			params->mongodbCli = NULL;
			mongoc_cleanup();
		}
	}
	else if (params->bitdbNode) { curl_global_cleanup(); }
}

/*
 * initializes MongoDB environment and client pool for multi-threaded scenarios
 * will set params->mongodbCliPool on success
 * must call CWG_cleanup_mongo_pool later on
 */
CW_STATUS initMongoPool(const char *mongodbAddr, struct CWG_params *params) {
	mongoc_init();

	bson_error_t error;  
	mongoc_uri_t *uri = mongoc_uri_new_with_error(mongodbAddr, &error);
	if (!uri) {
		fprintf(CWG_err_stream, "ERROR: cashgettools failed to parse provided MongoDB URI: %s\nMessage: %s\n", params->mongodb, error.message);
		mongoc_cleanup();
		return CW_CALL_NO;
	}

	params->mongodbCliPool = (void *)mongoc_client_pool_new(uri);
	mongoc_uri_destroy(uri);	
	if (!params->mongodbCliPool) {
		fprintf(CWG_err_stream, "ERROR: cashgettools failed to establish client with MongoDB\n");
		mongoc_cleanup();
		return CWG_FETCH_ERR;
	}
	mongoc_client_pool_set_error_api((mongoc_client_pool_t *)params->mongodbCliPool, MONGOC_ERROR_API_VERSION_2);
	mongoc_client_pool_set_appname((mongoc_client_pool_t *)params->mongodbCliPool, MONGODB_APPNAME);

	return CW_OK;
}

/*
 * cleans up MongoDB client pool (stored in params) and environment;
 * params->mongodbCliPool will be set NULL
 */
void cleanupMongoPool(struct CWG_params *params) {
	mongoc_client_pool_destroy((mongoc_client_pool_t *)params->mongodbCliPool);
	params->mongodbCliPool = NULL;
	mongoc_cleanup();
}
