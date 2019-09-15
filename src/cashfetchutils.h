#ifndef __CASHFETCHUTILS_H__
#define __CASHFETCHUTILS_H__

#include "cashgettools.h"

/* Fetch typing */
typedef enum FetchType {
        BY_TXID,
        BY_INTXID,
        BY_NAMETAG
} FETCH_TYPE;

/*
 * fetched hex data(s) at specified id(s) of specified type; fetch source is determined by implementation
 * writes txids (in order) to provided pointer (if not NULL), and writes all hex data (in order) to hexDataAll
 */
CW_STATUS fetchHexData(const char **ids, size_t count, FETCH_TYPE type, struct CWG_params *params, char **txids, char *hexDataAll);

/*
 * initializes for fetcher depending on implementation
 * should only be called from public functions that will get
 */
CW_STATUS initFetcher(struct CWG_params *params);

/*
 * cleans up for fetcher depending on implementation
 * should only be called from public functions that have called initFetcher()
 */
void cleanupFetcher(struct CWG_params *params);

/*
 * initializes MongoDB pool (used for thread-safety) if implementation supports MongoDB;
   otherwise, will return CW_CALL_NO
 */
CW_STATUS initMongoPool(const char *mongodbAddr, struct CWG_params *params);

/*
 * cleans up MongoDB pool (used for thread-safety) if implementation supports MongoDB;
   otherwise, does nothing
 */
void cleanupMongoPool(struct CWG_params *params);

#endif
