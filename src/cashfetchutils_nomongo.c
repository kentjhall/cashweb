#include "cashfetchutils.h"
#include "cashfetchhttputils.h"
#include "cashwebutils.h"

/*
 * fetched hex data(s) at specified id(s) of specified type; fetch source is determined by params
 * writes txids (in order) to provided pointer (if not NULL), and writes all hex data (in order) to hexDataAll
 */
CW_STATUS fetchHexData(const char **ids, size_t count, FETCH_TYPE type, struct CWG_params *params, char **txids, char *hexDataAll) {
	if (params->bitdbNode) { return fetchHexDataBitDBNode(ids, count, type, params->bitdbNode, params->requestLimit, txids, hexDataAll); }
	else if (params->restEndpoint) { return fetchHexDataREST(ids, count, type, params->restEndpoint, params->requestLimit, txids, hexDataAll); }
	else {
		fprintf(CWG_err_stream, "ERROR: BitDB HTTP endpoint address is set in cashgettools implementation\n");
		return CW_CALL_NO;
	}
}

/*
 * initializes for fetcher depending on params
 * should only be called from public functions that will get
 */
CW_STATUS initFetcher(struct CWG_params *params) {
	if (params->bitdbNode || params->restEndpoint) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		if (params->requestLimit) { srandom(time(NULL)); }
	}	
	else {
		fprintf(CWG_err_stream, "ERROR: cashgettools requires an HTTP endpoint to be specified (either BitDB or REST)\n");
		return CW_CALL_NO;
	}

	return CW_OK;
}

/*
 * cleans up for fetcher depending on params
 * should only be called from public functions that have called initFetcher()
 */
void cleanupFetcher(struct CWG_params *params) {
	if (params->bitdbNode) { curl_global_cleanup(); }
}

/*
 * initializes MongoDB environment and client pool for multi-threaded scenarios
 * will set params->mongodbCliPool on success
 * must call CWG_cleanup_mongo_pool later on
 */
CW_STATUS initMongoPool(const char *mongodbAddr, struct CWG_params *params) {
	return CW_CALL_NO;	
}

/*
 * cleans up MongoDB client pool (stored in params) and environment;
 * params->mongodbCliPool will be set NULL
 */
void cleanupMongoPool(struct CWG_params *params) {
	// does nothing
}
