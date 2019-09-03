#include "cashwebutils.h"
#include "cashsendtools.h"
#include <errno.h>
#include <fts.h>
#include <libbitcoinrpc/bitcoinrpc.h>

/* general constants */
#define LINE_BUF 150
#define ERR_WAIT_CYCLE 5
#define ERR_MSG_BUF 50
#define RECOVERY_INFO_BUF 15

/* tx sending constants */
#define B_ERR_MSG_BUF 50
#define B_ADDRESS_BUF 75
#define B_AMNT_STR_BUF 32
#define TX_AMNT_SATOSHIS(amnt_satoshis) ((double)amnt_satoshis/100000000)
#define TX_OP_RETURN "6a"
#define TX_OP_PUSHDATA1 "4c"
#define TX_OP_PUSHDATA1_THRESHOLD 75
#define TX_DUST_AMNT TX_AMNT_SATOSHIS(545)
#define TX_AMNT_SMALLEST TX_AMNT_SATOSHIS(1)
#define TX_TINYCHANGE_AMNT (TX_DUST_AMNT + TX_AMNT_SMALLEST)
#define TX_MAX_0CONF_CHAIN 25
#define TX_BASE_SZ 10
#define TX_INPUT_SZ 148
#define TX_OUTPUT_SZ 34
#define TX_DATA_BASE_SZ 10
#define TX_SZ_CAP 100000

/* stream for logging errors; defaults to stderr */
FILE *CWS_err_stream = NULL;
#define CWS_err_stream (CWS_err_stream ? CWS_err_stream : stderr)
#define perror(str) fprintf(CWS_err_stream, str": %s\n", errno ? strerror(errno) : "No errno")

/* stream for logging progress/details; defaults to stderr */
FILE *CWS_log_stream = NULL;
#define CWS_log_stream (CWS_log_stream ? CWS_log_stream : stderr)

/* rpc method identifiers */
typedef enum RpcMethod {
	RPC_M_GETBALANCE,
	RPC_M_GETUNCONFIRMEDBALANCE,
	RPC_M_LISTUNSPENT,
	RPC_M_LISTUNSPENT_0,
	RPC_M_GETRAWCHANGEADDRESS,
	RPC_M_ESTIMATEFEE,
	RPC_M_CREATERAWTRANSACTION,
	RPC_M_SIGNRAWTRANSACTIONWITHWALLET,
	RPC_M_SENDRAWTRANSACTION,
	RPC_M_GETRAWTRANSACTION,
	RPC_M_LOCKUNSPENT,
	RPC_METHODS_COUNT
} RPC_METHOD_ID;

/*
 * struct for carrying around UTXO specifiers
 */
struct CWS_utxo {
	char txid[CW_TXID_CHARS+1];
	int vout;
};

/*
 * initializes given struct CWS_utxo
 */
static inline void init_CWS_utxo(struct CWS_utxo *u, const char *txid, int vout);

/*
 * initializes given struct CWS_utxo to be used for revisioning
 */
static inline void init_rev_CWS_utxo(struct CWS_utxo *u, const char *txid);

/*
 * copies struct CWS_utxo values from source to dest
 */
static inline void copy_CWS_utxo(struct CWS_utxo *dest, struct CWS_utxo *source);

/*
 * struct for carrying initialized bitcoinrpc_cl_t and bitcoinrpc_methods; for internal use by cashsendtools
 * cli: RPC client struct
 * methods: array of RPC method structs; contains only methods to be used by cashsendtools
 * txsToSend: the expected number of TXs to be send; used for proactive UTXO creation;
 	      if left at zero, no extra UTXOs will be created in advance
 * reservedUtxos: JSON array of UTXOs reserved for send, both existing and newly created
 * reservedUtxodCount: maintains a count for the number of reserved UTXOs
 * forceTinyChangeLast: if set true, will ensure a tiny change output is created on last TX
 * forceInputUtxoLast: if set, will force given UTXO as first input for last TX
 * revisionLocks: contains json structure with details on unspents locked for revisioning
 * revLocksPath: path to json file containing revision locks
 * costCount: tracks how much has been spent throughout send
 * txCount: tracks how many TXs have been sent throughout send
 * justCounting: set to true if just counting cost/TXs; nothing is actually to be sent
 * justTxCounting: set to true if just counting TXs; nothing is actually to be sent
 * errMsg: carries last RPC error message
 */
struct CWS_rpc_pack {
	bitcoinrpc_cl_t *cli;
	struct bitcoinrpc_method *methods[RPC_METHODS_COUNT];
	size_t txsToSend;
	json_t *reservedUtxos;
	size_t reservedUtxosCount;
	bool forceTinyChangeLast;
	struct CWS_utxo *forceInputUtxoLast;
	const char *forceOutputAddrLast;
	json_t *revisionLocks;
	char *revLocksPath;
	double costCount;
	size_t txCount;
	bool justCounting;
	bool justTxCounting;
	char errMsg[ERR_MSG_BUF];
};

/*
 * attempts an RPC call via given struct CWS_rpc_pack with specified RPC method identifier and params
 * copies json response pointer to jsonResult; this needs to be freed by json_decref()
 */
static CW_STATUS rpcCallAttempt(struct CWS_rpc_pack *rp, RPC_METHOD_ID rpcMethodI, json_t *params, json_t **jsonResult);

/*
 * wrapper for rpcCallAttempt() to wait on connection error
 * will return appropriate status on any other error
 */
static CW_STATUS rpcCall(struct CWS_rpc_pack *rp, RPC_METHOD_ID rpcMethodI, json_t *params, json_t **jsonResult);

/*
 * check balance of wallet via RPC and writes to balance 
 */
static CW_STATUS checkBalance(struct CWS_rpc_pack *rp, double *balance);

/*
 * locks/unlocks unspents as per given json array revisionLocks via RPC
 * this function in no way interacts with revision locks file in data directory, but is built to interface with its data elements in form passed json array
 */
static CW_STATUS lockRevisionUnspents(json_t *revisionLocks, bool unlock, struct CWS_rpc_pack *rp);

/*
 * locks/unlocks (as per bool unlock) given utxo to prevent/allow future use for revisioning purposes, storing this information in data directory
 * if locking, both name and utxo must be specified; behavior is undefined otherwise
 * if unlocking, name or utxo must be specified; behavior is undefined otherwise;
   if non-empty name string is provided, will unlock by name and write utxo details to provided utxo memory location (pass NULL if not desired);
   if only utxo is provided, will unlock by matching utxo details and write corresponding name to provided name memory location (pass NULL if not desired)
 * returns CW_CALL_NO if attempting to lock existing name/utxo or unlock non-existant, or otherwise appropriate status code
 */
static CW_STATUS setRevisionLock(char *name, struct CWS_utxo *utxo, bool unlock, struct CWS_params *csp, struct CWS_rpc_pack *rp);

/*
 * calculates data size in tx for data of length hexDataLen by accounting for extra opcodes
 * writes true/false to extraByte for whether or not an extra byte was needed
 */
static int txDataSize(int hexDataLen, bool *extraByte);

/*
 * comparator for sorting UTXOs by amount (ascending) via qsort
 */
static inline int utxoComparator(const void *p1, const void *p2);

/*
 * attempts to send tx with OP_RETURN data as per hexDatas (can be multiple pushdatas) via RPC
 * specify useUnconfirmed for whether or not 0-conf unspents are to be used
 * writes resultant txid to resTxid
 */
static CW_STATUS sendTxAttempt(const char **hexDatas, size_t hexDatasC, bool isLast, struct CWS_rpc_pack *rp, bool useUnconfirmed, bool sameFee, char *resTxid);

/*
 * wrapper for sendTxAttempt() to handle errors involving insufficient balance, confirmations, and fees
 * will return appropriate status on any other error
 */
static CW_STATUS sendTx(const char **hexDatas, size_t hexDatasC, bool isLast, struct CWS_rpc_pack *rp, char *resTxid);

/*
 * appends struct CW_file_metadata values to given hex string
 */
static CW_STATUS hexAppendMetadata(struct CW_file_metadata *md, char *hexStr);

/*
 * attempts to save information from recoveryData to stream specified in params
 * will also save savedTreeDepth, cwType, and maxTreeDepth to this stream
 * savedTreeDepth indicates the depth reached before failure; with current implementation,
   anything sent after this will be lost (may make more robust in the future)
 * user will need to call rewind() afterward for reading from recovery stream
 * returns true on success
 */
static bool saveRecoveryData(FILE *recoveryData, int savedTreeDepth, struct CWS_params *params);

/*
 * sends file from stream fp as chain of TXs via RPC and writes resultant txid to resTxid
 * constructs/appends appropriate file metadata with cwType and treeDepth
 */
static CW_STATUS sendFileChain(FILE *fp, char *pdHexStr, struct CWS_rpc_pack *rp, CW_TYPE cwType, int treeDepth, char *resTxid);

/*
 * sends layer of file tree (i.e. all TXs at same depth) from stream fp via RPC; writes all txids to treeFp
 * if only one TX is sent, determines that this is root and constructs/appends appropriate file metadata with cwType and depth
 * writes number of TXs sent to numTxs
 */
static CW_STATUS sendFileTreeLayer(FILE *fp, char *pdHexStr, struct CWS_rpc_pack *rp, CW_TYPE cwType, int depth, int *numTxs, FILE *treeFp);

/*
 * sends file from stream fp as tree of TXs via RPC and writes resultant txid to resTxid,
   or in case of failure, writes the amount of funds lost (accounts for saved recovery data, so may amount to less than accrued cost)
 * uses params for passing cwType, handling maxTreeDepth, and saving to recoveryStream in case of failure
 * if maxTreeDepth is reached, will divert to sendFileChain() for sending as a chain of tree root TXs
 */
static CW_STATUS sendFileTree(FILE *fp, char *pdHexStr, struct CWS_rpc_pack *rp, struct CWS_params *params, int depth, double *fundsLost, char *resTxid);

/*
 * sends file from stream (in accordance with params) via RPC and writes resultant txid to resTxid,
   or in case of failure, writes the amount of funds lost (accounts for saved recovery data, so may amount to less than accrued cost)
 * starting depth (sDepth) can be specified in case of sending from recovery data
 * pdHexStr is hex string for pushdata to be added to starting tx; can be set NULL if none
 */
static CW_STATUS sendFileFromStream(FILE *stream, int sDepth, char *pdHexStr, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, double *fundsLost, char *resTxid);

/*
 * sends file at given path (in accordance with params) via RPC and writes resultant txid to resTxid,
   or in case of failure, writes the amount of funds lost (accounts for saved recovery data, so may amount to less than accrued cost)
 */
static CW_STATUS sendFileFromPath(const char *path, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, double *fundsLost, char *resTxid);

/*
 * recursively scans directory to determine the number of sendable files and writes to count
 */
static CW_STATUS scanDirFileCount(char const *ftsPathArg[], int *count);

/*
 * sends directory at given path (in accordance with params) via RPC and writes resultant txid to resTxid,
   or in case of failure, writes the amount of funds lost (accounts for saved recovery data, so may amount to less than accrued cost)
 * all files in directory are sent with given params; will send directory index at end unless otherwise specified by params
 */
static CW_STATUS sendDirFromPath(const char *path, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, double *fundsLost, char *resTxid);

/*
 * wrapper function for choosing between sendFileFromPath or sendDirFromPath based on specified asDir
 */
static inline CW_STATUS sendFromPath(const char *path, bool asDir, struct CWS_rpc_pack *rp, struct CWS_params *csp, double *fundsLost, char *resTxid);

/*
 * initializes RPC environment, RPC client (as per credentials in params), and necessary RPC methods
 * RPC client pointer is copied to rpcPack->cli, and RPC methods pointer is copied to rpcPack->methods
 * should only be called from public functions that will send
 */
static CW_STATUS initRpc(struct CWS_params *params, struct CWS_rpc_pack *rpcPack);

/*
 * cleans up RPC environment, as well as RPC client/methods in rpcPack (if present)
 * should only be called from public functions that have called initRpc()
 */
static void cleanupRpc(struct CWS_rpc_pack *rpcPack);

/*
 * struct CWS_sender stores a send function pointer and its arguments; strictly for internal use by cashsendtools
 * really only exists to avoid some repetitive code
 */
struct CWS_sender {
	CW_STATUS (*fromStream) (FILE *, int, char *, struct CWS_rpc_pack *, struct CWS_params *, double *, char *);
	CW_STATUS (*fromPath) (const char *, bool, struct CWS_rpc_pack *, struct CWS_params *, double *, char *);
	FILE *stream;
	int sDepth;
	char *pdHexStr;
	const char *path;
	bool asDir;
	struct CWS_rpc_pack *rp;
	struct CWS_params *csp;
};

/*
 * initializes given struct CWS_sender for a stream send
 */
static inline void init_CWS_sender_for_stream(struct CWS_sender *css, FILE *stream, int sDepth, char *pdHexStr, struct CWS_rpc_pack *rp, struct CWS_params *csp);

/*
 * initializes given struct CWS_sender for a path send
 */
static inline void init_CWS_sender_for_path(struct CWS_sender *css, const char *path, bool asDir, struct CWS_rpc_pack *rp, struct CWS_params *csp);

/*
 * runs through send function specified in given struct CWS_sender without actually sending, to count both cost and txid count
 * writes to txCount and costCount
 */
static CW_STATUS countBySender(struct CWS_sender *sender, size_t *txCount, double *costCount);

/*
 * sends by send function specified in given struct CWS_sender with arguments
 * writes to both fundsLost and resTxid
 */
static CW_STATUS sendBySender(struct CWS_sender *sender, double *fundsLost, char *resTxid);

/* ------------------------------------- PUBLIC ------------------------------------- */

CW_STATUS CWS_send_from_path(const char *path, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	struct stat st;
	if (stat(path, &st) != 0) { perror("stat() failed"); return CW_SYS_ERR; }

	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }
	
	struct CWS_sender sender;
	init_CWS_sender_for_path(&sender, path, S_ISDIR(st.st_mode), &rpcPack, params);

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
	} else { rpcPack.txsToSend = params->fragUtxos; }

	status = sendBySender(&sender, fundsLost, resTxid);
	
	cleanup:
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costCount; }
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_estimate_cost_from_path(const char *path, struct CWS_params *params, size_t *txCount, double *costEstimate) {
	struct stat st;
	if (stat(path, &st) != 0) { perror("stat() failed"); return CW_SYS_ERR; }
	
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }	

	struct CWS_sender sender;
	init_CWS_sender_for_path(&sender, path, S_ISDIR(st.st_mode), &rpcPack, params);

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
	} else { rpcPack.txsToSend = params->fragUtxos; }

	status = countBySender(&sender, txCount, costEstimate);

	cleanup:
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_from_stream(FILE *stream, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	// read stream into tmpfile to ensure it can be rewinded
	FILE *streamCopy;
	if ((streamCopy = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto cleanup; }
	if (copyStreamData(streamCopy, stream) != COPY_OK) { status = CW_SYS_ERR; goto cleanup; }
	rewind(streamCopy);

	struct CWS_sender sender;
	init_CWS_sender_for_stream(&sender, streamCopy, 0, NULL, &rpcPack, params);

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
	} else { rpcPack.txsToSend = params->fragUtxos; }
	rewind(streamCopy);

	status = sendBySender(&sender, fundsLost, resTxid);
	
	cleanup:
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costCount; }
		if (streamCopy) { fclose(streamCopy); }
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_estimate_cost_from_stream(FILE *stream, struct CWS_params *params, size_t *txCount, double *costEstimate) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }	

	// read stream into tmpfile to ensure it can be rewinded
	FILE *streamCopy;
	if ((streamCopy = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto cleanup; }
	if (copyStreamData(streamCopy, stream) != COPY_OK) { status = CW_SYS_ERR; goto cleanup; }
	rewind(streamCopy);

	struct CWS_sender sender;
	init_CWS_sender_for_stream(&sender, streamCopy, 0, NULL, &rpcPack, params);

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
		rewind(streamCopy);
	} else { rpcPack.txsToSend = params->fragUtxos; }

	status = countBySender(&sender, txCount, costEstimate);

	cleanup:
		if (streamCopy) { fclose(streamCopy); }
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_from_recovery_stream(FILE *recoveryStream, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	FILE *streamCopy = NULL;

	struct DynamicMemory line;
	initDynamicMemory(&line);

	// get data at beginning of recovery stream
	int readlineStatus;
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) { status = CW_SYS_ERR; goto cleanup; }
	params->cwType = atoi(line.data);
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) { status = CW_SYS_ERR; goto cleanup; }
	params->maxTreeDepth = atoi(line.data);
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) { status = CW_SYS_ERR; goto cleanup; }
	int depth = atoi(line.data);

	// read stream into tmpfile to ensure it can be rewinded
	if ((streamCopy = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto cleanup; }
	if (copyStreamData(streamCopy, recoveryStream) != COPY_OK) { status = CW_SYS_ERR; goto cleanup; }
	rewind(streamCopy);

	struct CWS_sender sender;
	init_CWS_sender_for_stream(&sender, streamCopy, depth, NULL, &rpcPack, params);

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
	} else { rpcPack.txsToSend = params->fragUtxos; }
	rewind(streamCopy);

	status = sendBySender(&sender, fundsLost, resTxid);
		
	cleanup:
		if (ferror(recoveryStream)) { perror("fgets() failed on recovery stream"); status = CW_SYS_ERR; }
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costCount; }
		if (streamCopy) { fclose(streamCopy); }
		freeDynamicMemory(&line);
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_estimate_cost_from_recovery_stream(FILE *recoveryStream, struct CWS_params *params, size_t *txCount, double *costEstimate) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	FILE *streamCopy = NULL;
	struct DynamicMemory line;
	initDynamicMemory(&line);

	// read through first few lines to get to depth
	int readlineStatus;
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK ||
	    (readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK ||
	    (readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) {
		if (ferror(recoveryStream)) { perror("fgets() failed on recovery stream"); }
		status = CW_SYS_ERR;
		goto cleanup;
	}
	int depth = atoi(line.data);

	// read stream into tmpfile to ensure it can be rewinded
	if ((streamCopy = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto cleanup; }
	if (copyStreamData(streamCopy, recoveryStream) != COPY_OK) { status = CW_SYS_ERR; goto cleanup; }
	rewind(streamCopy);

	struct CWS_sender sender;
	init_CWS_sender_for_stream(&sender, streamCopy, depth, NULL, &rpcPack, params);

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
		rewind(streamCopy);
	} else { rpcPack.txsToSend = params->fragUtxos; }

	status = countBySender(&sender, txCount, costEstimate);

	cleanup:
		if (streamCopy) { fclose(streamCopy); }
		freeDynamicMemory(&line);
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_nametag(const char *name, const CW_OPCODE *script, size_t scriptSz, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	if (!CW_is_valid_name(name)) {
		fprintf(CWS_err_stream, "CWS_send_nametag provided with name that is too long (maximum %lu characters)\n", CW_NAME_MAX_LEN);
		return CW_CALL_NO;
	}

	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }	

	FILE *scriptStream = NULL;	

	// construct hex string for name
	char cwName[strlen(CW_NAMETAG_PREFIX) + strlen(name) + 1]; cwName[0] = 0;
	strcat(cwName, CW_NAMETAG_PREFIX);
	strcat(cwName, name);
	size_t cwNameLen = strlen(cwName);
	char cwNameHexStr[cwNameLen*2];
	byteArrToHexStr(cwName, cwNameLen, cwNameHexStr);

	// write script byte data to tmpfile
	if ((scriptStream = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto cleanup; }
	if (fwrite(script, scriptSz, 1, scriptStream) < 1) { perror("fwrite() failed"); status = CW_SYS_ERR; goto cleanup; }
	rewind(scriptStream);	

	params->cwType = CW_T_FILE;
	struct CWS_sender sender;
	init_CWS_sender_for_stream(&sender, scriptStream, 0, cwNameHexStr, &rpcPack, params);

	// force extra tiny change unspent for future revisions
	if (!immutable) { rpcPack.forceTinyChangeLast = true; }	

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto cleanup; }
		rewind(scriptStream);
	} else { rpcPack.txsToSend = params->fragUtxos; }

	if ((status = sendBySender(&sender, NULL, resTxid)) == CW_CALL_NO) {
		fprintf(CWS_err_stream, "CWS_send_nametag attempted to process name that is too long (%zu characters); problem with cashsendtools\n", strlen(name));
	}
	if (status != CW_OK) { goto cleanup; }

	// lock resulting revisioning utxo if needed
	if (!immutable && !rpcPack.forceOutputAddrLast) {
		struct CWS_utxo lock;
		init_rev_CWS_utxo(&lock, resTxid);
		if ((status = setRevisionLock((char *)name, &lock, false, params, &rpcPack)) == CW_CALL_NO) {
			fprintf(CWS_err_stream, "Failed to lock revisioning utxo, name is already locked; check %s in data directory\n", CW_DATADIR_REVISIONS_FILE);
		} else if (status != CW_OK) {
			fprintf(CWS_err_stream, "Revisioning utxo lock failed; this may need to be done manually, check %s in data directory\n", CW_DATADIR_REVISIONS_FILE);
			goto cleanup;
		}	
	}

	cleanup:
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costCount; }
		if (scriptStream) { fclose(scriptStream); }
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_standard_nametag(const char *name, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	// construct script byte string
	CW_OPCODE scriptBytes[FILE_DATA_BUF + (strlen(attachId)-CW_NAMETAG_PREFIX_LEN)]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	if (!CWS_gen_script_writefrom_id(attachId, scriptPtr, &scriptSz)) { fprintf(CWS_err_stream, "invalid attachId provided for nametag/revisioning\n"); return CW_CALL_NO; }

	return CWS_send_nametag(name, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

CW_STATUS CWS_send_revision(const char *revTxid, const CW_OPCODE *script, size_t scriptSz, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	if (!CW_is_valid_txid(revTxid)) { fprintf(CWS_err_stream, "CWS_send_revision provided with revision txid of invalid format\n"); return CW_CALL_NO; }

	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }	

	FILE *scriptStream = NULL;

	// declare array (string) before goto statements (some compilers give me shit even though it's fixed-length)
	char name[CW_NAME_MAX_LEN+1]; name[0] = 0;

	// write script byte data to tmpfile
	if ((scriptStream = tmpfile()) == NULL) { perror("tmpfile() failed"); status = CW_SYS_ERR; goto cleanup; }
	if (fwrite(script, scriptSz, 1, scriptStream) < 1) { perror("fwrite() failed"); status = CW_SYS_ERR; goto cleanup; }
	rewind(scriptStream);	

	params->cwType = CW_T_FILE;
	struct CWS_sender sender;
	init_CWS_sender_for_stream(&sender, scriptStream, 0, NULL, &rpcPack, params);	

	// force extra tiny change unspent for future revisions
	if (!immutable) { rpcPack.forceTinyChangeLast = true; }

	// unlock specified utxo to be used as input, and copy corresponding name to memory
	struct CWS_utxo inUtxo;
	init_rev_CWS_utxo(&inUtxo, revTxid);
	if ((status = setRevisionLock(name, &inUtxo, true, params, &rpcPack)) == CW_CALL_NO) {
		fprintf(CWS_err_stream, "Revision txid is not stored as a revision lock; check %s in data directory", CW_DATADIR_REVISIONS_FILE);
		goto cleanup;
	} else if (status != CW_OK) { goto relock; }

	// set specified utxo as forced input for send	
	rpcPack.forceInputUtxoLast = &inUtxo;

	// analyze UTXO requirements in advance; writes to rpcPack
	if (params->fragUtxos == 1) {
		if ((status = countBySender(&sender, &rpcPack.txsToSend, NULL)) != CW_OK) { goto relock; }
		rewind(scriptStream);
	} else { rpcPack.txsToSend = params->fragUtxos; }

	if ((status = sendBySender(&sender, NULL, resTxid)) == CWS_INPUTS_NO) {
		fprintf(CWS_err_stream, "RPC reporting bad UTXO(s); check that UTXO specified for CWS_send_revision is owned by this wallet\n");
	} else if (status != CW_OK) { goto relock; }

	struct CWS_utxo resUtxo;
	init_rev_CWS_utxo(&resUtxo, resTxid);
	if (!immutable && !rpcPack.forceOutputAddrLast) {
		if ((status = setRevisionLock(name, &resUtxo, false, params, &rpcPack)) == CW_CALL_NO) {
			fprintf(CWS_err_stream, "Failed to lock revisioning utxo, name is already locked; check %s in data directory; problem with cashsendtools\n", CW_DATADIR_REVISIONS_FILE);
		} else if (status != CW_OK) {
			fprintf(CWS_err_stream, "Revisioning utxo lock failed; this may need to be done manually, check %s in data directory\n", CW_DATADIR_REVISIONS_FILE);
		}	
	}		
	goto cleanup;

	relock:
		// attempt to re-lock unspent in case of failure
		if (status != CW_OK) { setRevisionLock(name, &inUtxo, false, params, &rpcPack); }

	cleanup:
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costCount; }
		if (scriptStream) { fclose(scriptStream); }
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_replace_revision(const char *revTxid, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	CW_OPCODE scriptBytes[FILE_DATA_BUF + (strlen(attachId)-CW_NAMETAG_PREFIX_LEN)]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	if (!CWS_gen_script_writefrom_id(attachId, scriptPtr, &scriptSz)) { fprintf(CWS_err_stream, "invalid attachId provided for nametag/revisioning\n"); return CW_CALL_NO; }
	scriptPtr[scriptSz++] = CW_OP_TERM;

	return CWS_send_revision(revTxid, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

CW_STATUS CWS_send_prepend_revision(const char *revTxid, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	CW_OPCODE scriptBytes[FILE_DATA_BUF + (strlen(attachId)-CW_NAMETAG_PREFIX_LEN)]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	if (!CWS_gen_script_writefrom_id(attachId, scriptPtr, &scriptSz)) { fprintf(CWS_err_stream, "invalid attachId provided for nametag/revisioning\n"); return CW_CALL_NO; }

	return CWS_send_revision(revTxid, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

CW_STATUS CWS_send_append_revision(const char *revTxid, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	CW_OPCODE scriptBytes[FILE_DATA_BUF + (strlen(attachId)-CW_NAMETAG_PREFIX_LEN)]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	scriptPtr[scriptSz++] = CW_OP_WRITEFROMPREV;
	if (!CWS_gen_script_writefrom_id(attachId, scriptPtr, &scriptSz)) { fprintf(CWS_err_stream, "invalid attachId provided for nametag/revisioning\n"); return CW_CALL_NO; }
	scriptPtr[scriptSz++] = CW_OP_TERM;

	return CWS_send_revision(revTxid, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

CW_STATUS CWS_send_insert_revision(const char *revTxid, size_t bytePos, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	CW_OPCODE scriptBytes[FILE_DATA_BUF + (strlen(attachId)-CW_NAMETAG_PREFIX_LEN)]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	scriptPtr[scriptSz++] = CW_OP_STOREFROMPREV;
	CWS_gen_script_push_int(bytePos-1, scriptPtr, &scriptSz);
	scriptPtr[scriptSz++] = CW_OP_WRITESOMEFROMSTORED;
	if (!CWS_gen_script_writefrom_id(attachId, scriptPtr, &scriptSz)) { fprintf(CWS_err_stream, "invalid attachId provided for nametag/revisioning\n"); return CW_CALL_NO; }
	scriptPtr[scriptSz++] = CW_OP_WRITEFROMSTORED;
	scriptPtr[scriptSz++] = CW_OP_DROPSTORED;
	scriptPtr[scriptSz++] = CW_OP_TERM;

	return CWS_send_revision(revTxid, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

CW_STATUS CWS_send_delete_revision(const char *revTxid, size_t startPos, size_t bytesToDel, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	if (bytesToDel < 1) { fprintf(CWS_err_stream, "CWS_send_delete_revision provided with invalid number of bytes to delete\n"); return CW_CALL_NO; }

	CW_OPCODE scriptBytes[FILE_DATA_BUF]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	scriptPtr[scriptSz++] = CW_OP_STOREFROMPREV;
	CWS_gen_script_push_int(startPos-1, scriptPtr, &scriptSz);
	scriptPtr[scriptSz++] = CW_OP_WRITESOMEFROMSTORED;
	CWS_gen_script_push_int(CW_SEEK_CUR, scriptPtr, &scriptSz);
	CWS_gen_script_push_int(bytesToDel, scriptPtr, &scriptSz);
	scriptPtr[scriptSz++] = CW_OP_SEEKSTORED;
	scriptPtr[scriptSz++] = CW_OP_WRITEFROMSTORED;
	scriptPtr[scriptSz++] = CW_OP_DROPSTORED;
	scriptPtr[scriptSz++] = CW_OP_TERM;

	return CWS_send_revision(revTxid, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

CW_STATUS CWS_send_empty_revision(const char *revTxid, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid) {
	CW_OPCODE scriptBytes[FILE_DATA_BUF]; CW_OPCODE *scriptPtr = scriptBytes;
	size_t scriptSz = 0;
	CWS_gen_script_standard_start(rvp, params, scriptPtr, &scriptSz);
	if (!scriptSz) { scriptPtr[scriptSz++] = CW_OP_PUSHNO; }

	return CWS_send_revision(revTxid, scriptBytes, scriptSz, rvp->immutable, params, fundsUsed, resTxid);
}

void CWS_gen_script_push_int(uint32_t val, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	if (val <= UINT8_MAX) {
		scriptPtr[((*scriptSz))++] = CW_OP_PUSHCHAR;
		scriptPtr[(*scriptSz)++] = (CW_OPCODE)val;
	}
	else if (val <= UINT16_MAX) {
		scriptPtr[(*scriptSz)++] = CW_OP_PUSHSHORT;
		uint16_t len = (uint16_t)val;
		int16ToNetByteArr(len, scriptPtr + *scriptSz); *scriptSz += sizeof(len);
	}
	else {
		scriptPtr[(*scriptSz)++] = CW_OP_PUSHINT;
		uint32_t len = (uint32_t)val;
		int32ToNetByteArr(len, scriptPtr + *scriptSz); *scriptSz += sizeof(len);
	}
}

void CWS_gen_script_push_str(const char *str, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	size_t strLen = strlen(str);	

	if (strLen <= CW_OP_PUSHSTR) {
		scriptPtr[(*scriptSz)++] = (CW_OPCODE)strLen;
	} else {
		CWS_gen_script_push_int(strLen, scriptPtr, scriptSz);
		scriptPtr[(*scriptSz)++] = CW_OP_PUSHSTRX;
	}

	for (size_t i=0; i<strLen; i++) { scriptPtr[(*scriptSz)++] = (CW_OPCODE)str[i]; }
}


void CWS_gen_script_writefrom_nametag(const char *name, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	CWS_gen_script_push_str(name, scriptPtr, scriptSz);
	scriptPtr[(*scriptSz)++] = CW_OP_WRITEFROMNAMETAG;
}

bool CWS_gen_script_writefrom_txid(const char *txid, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	char txidByteArr[CW_TXID_BYTES];
	if (hexStrToByteArr(txid, 0, txidByteArr) != CW_TXID_BYTES) { return false; }

	scriptPtr[(*scriptSz)++] = CW_OP_PUSHTXID;
	memcpy(scriptPtr + *scriptSz, txidByteArr, CW_TXID_BYTES); *scriptSz += CW_TXID_BYTES;	
	scriptPtr[(*scriptSz)++] = CW_OP_WRITEFROMTXID;

	return true;
}

bool CWS_gen_script_writefrom_id(const char *id, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	const char *name;
	int rev;
	if (CW_is_valid_nametag_id(id, &rev, &name)) {
		if (rev >= 0) { fprintf(CWS_err_stream, "nametag/revision scripting doesn't support attaching specific revision of nametag\n"); return false; }
		CWS_gen_script_writefrom_nametag(name, scriptPtr, scriptSz);
		return true;
	} else { return CW_is_valid_txid(id) ? CWS_gen_script_writefrom_txid(id, scriptPtr, scriptSz) : false; }
}

void CWS_gen_script_pathlink(const char *toReplace, const char *replacement, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	CWS_gen_script_push_str(replacement, scriptPtr, scriptSz);
	CWS_gen_script_push_str(toReplace, scriptPtr, scriptSz);
	scriptPtr[(*scriptSz)++] = CW_OP_WRITEPATHLINK;
}

void CWS_gen_script_standard_start(struct CWS_revision_pack *rvp, struct CWS_params *params, CW_OPCODE *scriptPtr, size_t *scriptSz) {
	params->revToAddr = rvp->transferAddr;
	if (!rvp->immutable) { scriptPtr[(*scriptSz)++] = CW_OP_NEXTREV; }
	if (rvp->pathToReplace && rvp->pathReplacement) { CWS_gen_script_pathlink(rvp->pathToReplace, rvp->pathReplacement, scriptPtr, scriptSz); }
}

CW_STATUS CWS_wallet_lock_revision_utxos(struct CWS_params *params) {
	struct CWS_rpc_pack rpcPack;
	CW_STATUS status = initRpc(params, &rpcPack); // initRpc will always lock revision utxos via RPC
	cleanupRpc(&rpcPack);
	return status;
}

CW_STATUS CWS_set_revision_lock(const char *name, const char *revTxid, bool unlock, struct CWS_params *csp) {
	if (name && !CW_is_valid_name(name)) {
		fprintf(CWS_err_stream, "CWS_set_revision_lock provided with name that is too long (maximum %lu characters)\n", CW_NAME_MAX_LEN);
		return CW_CALL_NO;
	}
	
	CW_STATUS status;

	struct CWS_rpc_pack rp;
	if ((status = initRpc(csp, &rp)) != CW_OK) { cleanupRpc(&rp); return status; }

	char rName[CW_NAME_MAX_LEN+1]; rName[0] = 0;
	if (name) { strncat(rName, name, CW_NAME_MAX_LEN); }

	struct CWS_utxo revUtxo;
	if (revTxid) { init_rev_CWS_utxo(&revUtxo, revTxid); }
	
	if ((status = setRevisionLock(name ? rName : NULL, revTxid ? &revUtxo : NULL, unlock, csp, &rp)) == CW_CALL_NO) {
		if (unlock) {
			fprintf(CWS_err_stream, "Failed to unlock revisioning utxo, name and/or utxo is not stored as a revision lock; check %s in data directory\n", CW_DATADIR_REVISIONS_FILE);
		} else {
			fprintf(CWS_err_stream, "Failed to lock revisioning utxo, name and/or utxo is already locked; check %s in data directory\n", CW_DATADIR_REVISIONS_FILE);
		}
	}

	cleanupRpc(&rp);
	return status;
}

CW_STATUS CWS_get_stored_revision_txid_by_name(const char *name, struct CWS_params *csp, char *revTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rp;
	if ((status = initRpc(csp, &rp)) != CW_OK) { cleanupRpc(&rp); return status; }

	json_t *rUtxo = json_object_get(rp.revisionLocks, name);
	if (!rUtxo) { status = CW_CALL_NO; goto cleanup; }

	if (revTxid) {
		const char *rTxid = json_string_value(json_object_get(rUtxo, "txid"));
		int rVout = json_integer_value(json_object_get(rUtxo, "vout"));
		if (!rTxid || rVout != CW_REVISION_INPUT_VOUT) {
			fprintf(CWS_err_stream, "%s formatting is invalid; check file in data directory\n", CW_DATADIR_REVISIONS_FILE);
			status = CW_DATADIR_NO;
			goto cleanup;
		}
		revTxid[0] = 0; strncat(revTxid, rTxid, CW_TXID_CHARS);
	}

	cleanup:
		cleanupRpc(&rp);
		return status;
}

CW_STATUS CWS_set_cw_mime_type_by_extension(const char *fname, struct CWS_params *csp) {
	if (csp->datadir == NULL) { csp->datadir = CW_INSTALL_DATADIR_PATH; }

	CW_STATUS status = CW_OK;
	bool matched = false;

	// copies extension from fname to memory
	char extension[strlen(fname)+1]; extension[0] = 0;
	char *fnamePtr;
	if ((fnamePtr = strrchr(fname, '.')) == NULL) { strcat(extension, fname); } else { strcat(extension, fnamePtr+1); }
	
	// determine mime.types full path by cashweb protocol version and set datadir path
	int dataDirPathLen = strlen(csp->datadir);
	bool appendSlash = csp->datadir[dataDirPathLen-1] != '/';
	char mtFilePath[dataDirPathLen + 1 + strlen(CW_DATADIR_MIMETYPES_PATH) + strlen("CW65535_mime.types") + 1];
	snprintf(mtFilePath, sizeof(mtFilePath), "%s%s%sCW%u_mime.types", csp->datadir, appendSlash ? "/" : "", CW_DATADIR_MIMETYPES_PATH, CW_P_VER);

	// initialize data/file pointers before goto statements
	FILE *mimeTypes = NULL;
	struct DynamicMemory line;
	initDynamicMemory(&line);

	// checks for mime.types in data directory
	if (access(mtFilePath, R_OK) == -1) {
		status = CW_DATADIR_NO;
		goto cleanup;
	}

	// open protocol-specific mime.types file
	if ((mimeTypes = fopen(mtFilePath, "r")) == NULL) {
		fprintf(CWS_err_stream, "fopen() failed on path %s; unable to open cashweb mime.types: %s\n", mtFilePath, strerror(errno));
		status = CW_SYS_ERR;
		goto cleanup;
	}	

	// read mime.types file to match extension	
	char *lineDataStart;
	char *lineDataToken;
	CW_TYPE type = CW_T_MIMESET;	
	int readlineStatus;
	while ((readlineStatus = safeReadLine(&line, LINE_BUF, mimeTypes)) == READLINE_OK) {
		if (line.data[0] == '#') { continue; }
		++type;
		
		lineDataStart = line.data;
		while ((lineDataToken = strsep(&line.data, "\t ")) != NULL) {
			if (strcmp(lineDataToken, extension) == 0) {
				matched = true;
				csp->cwType = type;
				break;
			}
		}
		line.data = lineDataStart;
		if (matched) { break; }
	}
	if (ferror(mimeTypes)) { perror("fgets() failed on mime.types"); status = CW_SYS_ERR; goto cleanup; }
	else if (readlineStatus == READLINE_ERR) { status = CW_SYS_ERR; goto cleanup; }

	// defaults to CW_T_FILE if extension not matched
	if (!matched) { csp->cwType = CW_T_FILE; }

	cleanup:
		freeDynamicMemory(&line);
		if (mimeTypes) { fclose(mimeTypes); }
		return status;
}

CW_STATUS CWS_dirindex_json_to_raw(FILE *indexJsonFp, FILE *indexFp) {
	json_error_t e;
	json_t *indexJson;
	if ((indexJson = json_loadf(indexJsonFp, 0, &e)) == NULL) { fprintf(CWS_err_stream, "json_loadf() failed\nMessage: %s\n", e.text); return CW_SYS_ERR; }

	CW_STATUS status = CW_OK;

	size_t numEntries = json_object_size(indexJson);
	if (numEntries < 1) { fprintf(CWS_err_stream, "CWS_dirindex_json_to_raw provided with empty JSON data\n"); json_decref(indexJson); return CW_CALL_NO; }
	char txidsByteData[numEntries*CW_TXID_BYTES];

	size_t tCount = 0;
	size_t nCount = 0;
	const char *id;

	const char *path;
	json_t *idVal;
	json_object_foreach(indexJson, path, idVal) {
		char dirPath[strlen(path)+1+1]; dirPath[0] = 0;
		if (path[0] != '/') { strcat(dirPath, "/"); }
		strcat(dirPath, path);

		if (tCount+nCount >= numEntries) {
			fprintf(CWS_err_stream, "invalid numEntries calculated in CWS_dirindex_json_to_raw; problem with cashsendtools\n");
			status = CW_SYS_ERR;
			goto cleanup;
		}
		if ((id = json_string_value(idVal)) == NULL) {
			fprintf(CWS_err_stream, "CWS_dirindex_json_to_raw provided with empty JSON data\n");
			status = CW_CALL_NO;
			goto cleanup;
		}

		if (fprintf(indexFp, "%s\n", dirPath) < 0) { perror("fprintf() to indexFp failed"); status = CW_SYS_ERR; goto cleanup; }	
		if (CW_is_valid_nametag_id(id, NULL, NULL) || CW_is_valid_path_id(id, NULL, NULL)) {
			if (fprintf(indexFp, "%s\n", id) < 0) { perror("fprintf() to indexFp failed"); status = CW_SYS_ERR; goto cleanup; }	
			++nCount;
		}
		else {
			if (hexStrToByteArr(id, 0, txidsByteData+(tCount++ * CW_TXID_BYTES)) != CW_TXID_BYTES) {
				fprintf(CWS_err_stream, "CWS_dirindex_json_to_raw provided JSON contains invalid txid(s)\n");
				status = CW_CALL_NO;
				goto cleanup;
			}
		}
	}
	if (tCount+nCount < numEntries) {
		fprintf(CWS_err_stream, "invalid numEntries calculated in CWS_dirindex_json_to_raw; problem with cashsendtools\n");
		status = CW_SYS_ERR;
		goto cleanup;
	}

	if (fprintf(indexFp, "\n") < 0) { perror("fprintf() to indexFp failed"); status = CW_SYS_ERR; goto cleanup; }
	if (fwrite(txidsByteData, CW_TXID_BYTES, tCount, indexFp) < tCount) { perror("fwrite() to indexFp failed"); }

	cleanup:
		json_decref(indexJson);
		return status;
}

const char *CWS_errno_to_msg(int errNo) {
	switch (errNo) {
		case CW_DATADIR_NO:
			return "Unable to find proper cashwebtools data directory";
		case CW_CALL_NO:
			return "Bad call to cashsendtools function; may be bad implementation";
		case CW_SYS_ERR:
			return "There was an unexpected system error. This may be problem with cashsendtools";
		case CWS_INPUTS_NO:
			return "Invalid UTXOS in wallet causing mempool conflict";
		case CWS_RPC_NO:
			return "Received an unexpected RPC response error";
		case CWS_RPC_ERR:
			return "Failed to connect via RPC. Please ensure bitcoind is running at specified address, port is open, and credentials are correct";
		default:
			return "Unexpected error code. This is likely an issue with cashsendtools";
	}
}

/* ---------------------------------------------------------------------------------- */

static inline void init_CWS_utxo(struct CWS_utxo *u, const char *txid, int vout) {
	u->txid[0] = 0; strncat(u->txid, txid, CW_TXID_CHARS);
	u->vout = vout;
}

static inline void init_rev_CWS_utxo(struct CWS_utxo *u, const char *txid) {
	u->txid[0] = 0; strncat(u->txid, txid, CW_TXID_CHARS);
	u->vout = CW_REVISION_INPUT_VOUT;
}

static inline void copy_CWS_utxo(struct CWS_utxo *dest, struct CWS_utxo *source) {
	dest->txid[0] = 0; strncat(dest->txid, source->txid, CW_TXID_CHARS);
	dest->vout = source->vout;
}

static CW_STATUS rpcCallAttempt(struct CWS_rpc_pack *rp, RPC_METHOD_ID rpcMethodI, json_t *params, json_t **jsonResult) {
	bitcoinrpc_resp_t *bResponse = bitcoinrpc_resp_init();
	bitcoinrpc_err_t bError;
	if (!bResponse) { perror("bitcoinrpc_resp_init() failed"); return CW_SYS_ERR; }
	CW_STATUS status = CW_OK;

	struct bitcoinrpc_method *rpcMethod = rp->methods[rpcMethodI];
	if (!rpcMethod) { fprintf(CWS_err_stream, "rpc method (identifier %d) not initialized; probably an issue with cashsendtools\n", rpcMethodI);
			  status = CW_SYS_ERR; goto cleanup; }
	if (params != NULL && bitcoinrpc_method_set_params(rpcMethod, params) != BITCOINRPCE_OK) {
		fprintf(CWS_err_stream, "count not set params for rpc method (identifier %d); probably an issue with cashsendtools\n", rpcMethodI);
		status = CW_SYS_ERR;
		goto cleanup;
	}
	bitcoinrpc_call(rp->cli, rpcMethod, bResponse, &bError);	

	if (bError.code != BITCOINRPCE_OK) { fprintf(CWS_err_stream, "rpc error %d [%s]\n", bError.code, bError.msg); status = CWS_RPC_ERR; goto cleanup; }

	json_t *jsonResponse = bitcoinrpc_resp_get(bResponse);
	*jsonResult = json_object_get(jsonResponse, "result");
	if (json_is_null(*jsonResult)) {
		json_decref(*jsonResult);
		*jsonResult = json_object_get(jsonResponse, "error");
		rp->errMsg[0] = 0;
		strncat(rp->errMsg, json_string_value(json_object_get(*jsonResult, "message")), ERR_MSG_BUF-1);
		status = CWS_RPC_NO;
	} 
	json_incref(*jsonResult);
	json_decref(jsonResponse);

	cleanup:
		bitcoinrpc_resp_free(bResponse);
		return status;
}

static CW_STATUS rpcCall(struct CWS_rpc_pack *rp, RPC_METHOD_ID rpcMethodI, json_t *params, json_t **jsonResult) {
	bool printed = false;
	CW_STATUS status;
	while ((status = rpcCallAttempt(rp, rpcMethodI, params, jsonResult)) == CWS_RPC_ERR) {
		if (!printed) {
			fprintf(CWS_err_stream, "\nRPC request failed, please ensure bitcoind is running and configured correctly; retrying...\n");
			printed = true;
		}
		sleep(ERR_WAIT_CYCLE);
	}
	return status;
}

static CW_STATUS checkBalance(struct CWS_rpc_pack *rp, double *balance) {
	CW_STATUS status;
	json_t *jsonResult = NULL;

	status = rpcCall(rp, RPC_M_GETBALANCE, NULL, &jsonResult);
	if (jsonResult) {
		*balance = json_real_value(jsonResult);
		json_decref(jsonResult);
		jsonResult = NULL;
	}
	if (status != CW_OK) { return status; }

	status = rpcCall(rp, RPC_M_GETUNCONFIRMEDBALANCE, NULL, &jsonResult);
	if (jsonResult) {
		*balance += json_real_value(jsonResult);
		json_decref(jsonResult);
	}
	
	return status;
}

static CW_STATUS lockRevisionUnspents(json_t *revisionLocks, bool unlock, struct CWS_rpc_pack *rp) {
	json_t *jsonResult = NULL;
        json_t *params = NULL;
        json_t *utxos = NULL;

        if ((params = json_array()) == NULL || (utxos = json_array()) == NULL) {
                perror("json_array() failed");
                if (utxos) { json_decref(utxos); }
                if (params) { json_decref(params); }
                return CW_SYS_ERR;
        }

	const char *key;
	json_t *unspent;
	json_object_foreach(revisionLocks, key, unspent) { json_array_append(utxos, unspent); } 

        json_array_append_new(params, json_boolean(unlock));
        json_array_append(params, utxos);
        json_decref(utxos);

        CW_STATUS status = rpcCall(rp, RPC_M_LOCKUNSPENT, params, &jsonResult);
        if (jsonResult) { json_decref(jsonResult); }
        json_decref(params);
	if (status != CW_OK && strstr(rp->errMsg, "already locked")) { status = CW_OK; }

        return status;	
}

static CW_STATUS setRevisionLock(char *name, struct CWS_utxo *utxo, bool unlock, struct CWS_params *csp, struct CWS_rpc_pack *rp) {
	CW_STATUS status = CW_OK;

	const char *rName;
	const char *rTxid;
	int rVout;
	json_t *rUtxo;

	json_t *lock;
	json_t *locks;
	struct CWS_utxo lockUtxo;
	if (utxo) { copy_CWS_utxo(&lockUtxo, utxo); }
	else { init_CWS_utxo(&lockUtxo, "", 0); }

	if (name && name[0] != 0 && (rUtxo = json_object_get(rp->revisionLocks, name))) {
		if (unlock) {
			rTxid = json_string_value(json_object_get(rUtxo, "txid"));
			rVout = json_integer_value(json_object_get(rUtxo, "vout"));
			if (!rTxid || rVout != CW_REVISION_INPUT_VOUT) {
				fprintf(CWS_err_stream, "%s formatting is invalid; check file in data directory\n", CW_DATADIR_REVISIONS_FILE);
				return CW_DATADIR_NO;
			}

			init_CWS_utxo(&lockUtxo, rTxid, rVout);
			if (utxo) { copy_CWS_utxo(utxo, &lockUtxo); }

			json_object_del(rp->revisionLocks, name);
			goto lock;
		} else { return CW_CALL_NO; }
	}
	
	if (unlock && !utxo) { return CW_CALL_NO; }

	json_object_foreach(rp->revisionLocks, rName, rUtxo) {
		rTxid = json_string_value(json_object_get(rUtxo, "txid"));
		rVout = json_integer_value(json_object_get(rUtxo, "vout"));
		if (!rTxid || rVout != CW_REVISION_INPUT_VOUT) {
			fprintf(CWS_err_stream, "%s formatting is invalid; check file in data directory\n", CW_DATADIR_REVISIONS_FILE);
			return CW_DATADIR_NO;
		}

		if (strcmp(rTxid, utxo->txid) == 0 && rVout == utxo->vout) {
			if (unlock) {
				init_CWS_utxo(&lockUtxo, rTxid, rVout);
				if (name) { name[0] = 0; strncat(name, rName, CW_NAME_MAX_LEN); }
				json_object_del(rp->revisionLocks, rName);
				goto lock;
			} else { return CW_CALL_NO; }
		}
	}

	if (unlock) { return CW_CALL_NO; }

	lock:
	lock = json_object();
	if (!lock) { perror("json_object() failed"); return CW_SYS_ERR; }
	locks = json_object();
	if (!locks) { perror("json_object() failed"); json_decref(lock); return CW_SYS_ERR; }
	json_object_set_new(lock, "txid", json_string(lockUtxo.txid));
	json_object_set_new(lock, "vout", json_integer(lockUtxo.vout));
	json_object_set(locks, !unlock ? name : "", lock);
	json_decref(lock);

	if (!unlock) { json_object_update_missing(rp->revisionLocks, locks); }
	status = lockRevisionUnspents(locks, unlock, rp);
	json_decref(locks);

	if (status == CW_OK && json_dump_file(rp->revisionLocks, rp->revLocksPath, JSON_INDENT(4)) != 0) { perror("json_dump_file() failed"); return CW_SYS_ERR; }
	return status;
}

static int txDataSize(int hexDataLen, bool *extraByte) {
	int dataSize = hexDataLen/2;
	int added = dataSize > TX_OP_PUSHDATA1_THRESHOLD ? 2 : 1;
	if (extraByte != NULL) { *extraByte = added > 1; }
	return dataSize + added; 
}

static inline int utxoComparator(const void *p1, const void *p2) {
	const json_t *u1 = *(const json_t **)p1;
	const json_t *u2 = *(const json_t **)p2;

	double diff = json_real_value(json_object_get(u1, "amount")) - json_real_value(json_object_get(u2, "amount"));
	return diff < 0 ? -1 : 1;
}

static CW_STATUS sendTxAttempt(const char *hexDatas[], size_t hexDatasC, bool isLast, struct CWS_rpc_pack *rp, bool useUnconfirmed, bool sameFee, char *resTxid) {
	CW_STATUS status = CW_OK;
	json_t *jsonResult = NULL;
	json_t *params;
	
	// get estimated fee per byte and copy to memory; only checks once per program execution, or again on fee error
	static double feePerByte;
	if (!feePerByte || !sameFee) {
		if ((status = rpcCall(rp, RPC_M_ESTIMATEFEE, NULL, &jsonResult)) != CW_OK) {
			if (jsonResult) { json_decref(jsonResult); }
			return status;
		}
		feePerByte = json_real_value(jsonResult)/1000;
		json_decref(jsonResult);
		jsonResult = NULL;
	}

	// get unspent utxos if reservedUtxos don't already exist in rp;
	// otherwise, use the already-created utxos
	if (rp->reservedUtxos && rp->reservedUtxosCount <= 0) { json_decref(rp->reservedUtxos); rp->reservedUtxos = NULL; rp->reservedUtxosCount = 0; }
	json_t *unspents;
	size_t numUnspents;
	if (!rp->reservedUtxos || rp->reservedUtxosCount < rp->txsToSend) {
		if ((status = rpcCall(rp, useUnconfirmed ? RPC_M_LISTUNSPENT_0 : RPC_M_LISTUNSPENT, NULL, &unspents)) != CW_OK) {
			if (unspents) { json_decref(unspents); }
			return status;
		}
		if ((numUnspents = json_array_size(unspents)) == 0 && !rp->justCounting) {
			json_decref(unspents);
			return useUnconfirmed ? CWS_FUNDS_NO : CWS_CONFIRMS_NO;
		}	
	} else { unspents = rp->reservedUtxos; numUnspents = rp->reservedUtxosCount; }
	bool usedUnspents[numUnspents > 0 ? numUnspents : 1]; memset(usedUnspents, 0, numUnspents);
	double inputAmnts[numUnspents > 0 ? numUnspents : 1]; memset(inputAmnts, 0, numUnspents*sizeof(inputAmnts[0]));
	int inputIndices[numUnspents > 0 ? numUnspents : 1]; memset(inputIndices, 0, numUnspents*sizeof(inputIndices[0]));

	json_t *utxo;
	json_t *input;
	json_t *inputParams;
	json_t *outputParams;

	// calculate tx data size and prepare datas if more than one pushdata
	int txDataSz = 0;
	char txHexData[CW_TX_RAW_DATA_CHARS+1]; txHexData[0] = 0;
	int hexLen; int dataSz; bool extraByte; uint8_t szByte[1]; char szHex[2];
	for (int i=0; i<hexDatasC; i++) {
		txDataSz += txDataSize((hexLen = strlen(hexDatas[i])), &extraByte);
		if (hexDatasC > 1) {
			if ((dataSz = hexLen/2) > 255) {
				fprintf(CWS_err_stream, "sendTxAttempt() doesn't support data >255 bytes; cashsendtools may need revision if this standard has changed\n");
				
				if (!rp->reservedUtxos) { json_decref(unspents); }	
				return CW_SYS_ERR;
			}
			else if (extraByte) { strcat(txHexData, TX_OP_PUSHDATA1); }
			*szByte = (uint8_t)dataSz;
			byteArrToHexStr((const char *)szByte, 1, szHex);
			strcat(txHexData, szHex);
			strcat(txHexData, hexDatas[i]);
		}
	}		

	size_t reuseChangeOutCount;
	double reuseChangeAmnt;
	bool tinyChange = isLast && rp->forceTinyChangeLast;
	bool distributed = false;
	if (unspents != rp->reservedUtxos && rp->txsToSend > numUnspents && rp->txsToSend >= TX_MAX_0CONF_CHAIN) {
		if (useUnconfirmed && sameFee && !rp->justCounting) { fprintf(CWS_log_stream, "Distributing UTXOs..."); }
		distributed = true;
		reuseChangeOutCount = (rp->txsToSend - 1) - (numUnspents - 1);
		reuseChangeAmnt = feePerByte*(TX_BASE_SZ+TX_INPUT_SZ+TX_OUTPUT_SZ+TX_DATA_BASE_SZ+CW_TX_RAW_DATA_BYTES) + TX_TINYCHANGE_AMNT;

		// sort UTXOs by amount when to be used for distributing
		json_t *unspentsArr[numUnspents];	
		for (int i=0; i<numUnspents; i++) { unspentsArr[i] = json_array_get(unspents, i); json_incref(unspentsArr[i]); }
		json_array_clear(unspents);
		qsort(unspentsArr, numUnspents, sizeof(json_t *), &utxoComparator);
		for (int i=0; i<numUnspents; i++) { json_array_append_new(unspents, unspentsArr[i]); }
	} else {	
		reuseChangeOutCount = 0;
		reuseChangeAmnt = 0;
	}
	size_t changeOutCount = 1 + reuseChangeOutCount;
	double totalExtraChange = reuseChangeAmnt*reuseChangeOutCount;
	if (tinyChange) { ++changeOutCount; totalExtraChange += TX_TINYCHANGE_AMNT; }

	size_t size = TX_BASE_SZ + (TX_OUTPUT_SZ*changeOutCount) + TX_DATA_BASE_SZ + txDataSz;
	double totalAmnt = 0;
	double fee = TX_AMNT_SMALLEST; // arbitrary
	double changeAmnt = 0;	
	double changeLost = 0;	
	size_t inputsCount = 0;

	char txid[CW_TXID_CHARS+1];
	int vout;
	if ((inputParams = json_array()) == NULL) {
		perror("json_array() failed");
		if (unspents != rp->reservedUtxos) { json_decref(unspents); }	
		return CW_SYS_ERR;
	}	

	if (isLast && rp->forceInputUtxoLast) {
		// add forced input to inputParams first
		char *forceTxid = rp->forceInputUtxoLast->txid;
		int forceVout = rp->forceInputUtxoLast->vout;

		bool matched = false;
		json_t *utxo;
		const char *iTxid;
		int iVout;
		for (int i=numUnspents-1; i>=0; i--) {
			utxo = json_array_get(unspents, i);
			iTxid = json_string_value(json_object_get(utxo, "txid"));
			iVout = json_integer_value(json_object_get(utxo, "vout"));
			if (strcmp(iTxid, forceTxid) == 0 && iVout == forceVout) {
				txid[0] = 0;
				strncat(txid, iTxid, CW_TXID_CHARS);
				vout = iVout;
				if ((input = json_object()) == NULL) {
					perror("json_object() failed");
					json_decref(inputParams);
					return CW_SYS_ERR;
				}
				json_object_set_new(input, "txid", json_string(txid));
				json_object_set_new(input, "vout", json_integer(vout));
				usedUnspents[i] = true;
				inputIndices[inputsCount] = i;
				json_array_append(inputParams, input);
				json_decref(input);
				totalAmnt += (inputAmnts[inputsCount] = json_real_value(json_object_get(utxo, "amount")));
				size += TX_INPUT_SZ; 
				fee = feePerByte * size;
				++inputsCount;

				matched = true;
				break;
			}
		}

		if (!matched) {
			if (unspents != rp->reservedUtxos) { json_decref(unspents); }	
			return CWS_INPUTS_NO;
		}
	}

	// iterate through unspent utxos
	for (int i=numUnspents-1; i>=0; i--) {
		// pull utxo data
		utxo = json_array_get(unspents, i);

		// copy txid from utxo data to memory
		txid[0] = 0;
		strncat(txid, json_string_value(json_object_get(utxo, "txid")), CW_TXID_CHARS);

		// copy vout from utxo data to memory
		vout = json_integer_value(json_object_get(utxo, "vout"));

		// skip if saving for last tx
		if (rp->forceInputUtxoLast && strcmp(txid, rp->forceInputUtxoLast->txid) == 0 && vout == rp->forceInputUtxoLast->vout) { continue; }

		// construct input from txid and vout
		if ((input = json_object()) == NULL) {
			perror("json_object() failed");
			json_decref(inputParams);
			return CW_SYS_ERR;
		}
		json_object_set_new(input, "txid", json_string(txid));
		json_object_set_new(input, "vout", json_integer(vout));

		// set used unspent to be removed from stored array
		usedUnspents[i] = true;
		inputIndices[inputsCount] = i;

		// add input to input parameters
		json_array_append(inputParams, input);
		json_decref(input);
	
		// add amount from utxo data to total amount
		totalAmnt += (inputAmnts[inputsCount] = json_real_value(json_object_get(utxo, "amount")));

		// calculate size, fee, and change amount
		size += TX_INPUT_SZ; 
		fee = feePerByte * size;
		changeAmnt = totalAmnt - (totalExtraChange + fee);
		changeAmnt = changeAmnt > 0 ? changeAmnt : 0;
		// drop the change if less than cost of adding an additional input
		if (changeAmnt < feePerByte*(TX_INPUT_SZ)) { fee = feePerByte*(size-TX_OUTPUT_SZ); changeLost = changeAmnt; changeAmnt = 0; }

		++inputsCount;

		if (totalAmnt >= totalExtraChange + fee && (changeAmnt > TX_DUST_AMNT || changeAmnt == 0)) { break; }
	}		

	// give up if insufficient funds
	if (totalAmnt < fee + totalExtraChange) {
		if (!rp->justCounting) { json_decref(inputParams); return useUnconfirmed ? CWS_FUNDS_NO : CWS_CONFIRMS_NO; }
		else {
			// when just counting, will assume an extra input is to be added
			size += TX_INPUT_SZ; fee = feePerByte*size; changeLost = 0;
		}
	}	

	if (distributed) {
		// need to add to extra change outputs if there were too many inputs used
		// adds extra inputs as needed given additional outputs
		bool adjust = false;
		size_t inputsCountI;
		json_t *utxo;
		for (; reuseChangeOutCount <= (rp->txsToSend-1) - (numUnspents-inputsCount); ++reuseChangeOutCount) {
			adjust = true;
			totalExtraChange += reuseChangeAmnt;

			size += TX_OUTPUT_SZ;
			fee = feePerByte * size;

			inputsCountI = inputsCount;
			for (int i=numUnspents-1-inputsCountI; i >= 0; i--) {
				if (totalAmnt >= totalExtraChange + fee) { break; }
				
				// this is all basically duplicate code from the above loop; super lazy
				utxo = json_array_get(unspents, i);	
				txid[0] = 0;
				strncat(txid, json_string_value(json_object_get(utxo, "txid")), CW_TXID_CHARS);
				vout = json_integer_value(json_object_get(utxo, "vout"));
				if ((input = json_object()) == NULL) {
					perror("json_object() failed");
					json_decref(inputParams);
					return CW_SYS_ERR;
				}
				json_object_set_new(input, "txid", json_string(txid));
				json_object_set_new(input, "vout", json_integer(vout));
				usedUnspents[i] = true;
				json_array_append(inputParams, input);
				json_decref(input);
				totalAmnt += (inputAmnts[inputsCount] = json_real_value(json_object_get(utxo, "amount")));
				size += TX_INPUT_SZ; 
				fee = feePerByte * size;
				++inputsCount;
			}

			changeAmnt = totalAmnt - (totalExtraChange + fee); changeAmnt = changeAmnt > 0 ? changeAmnt : 0;
			if (changeAmnt < feePerByte*(TX_INPUT_SZ)) { fee = feePerByte*(size-TX_OUTPUT_SZ); changeLost = changeAmnt; changeAmnt = 0; }
		}
		if (adjust) { --reuseChangeOutCount; }
	}	
	
	if (reuseChangeOutCount) {
		// reduce number of extra change outputs if/while the size hits TX_SZ_CAP
		// also removes unnecessary inputs given reduced outputs
		bool removedIns = false;
		while (size >= TX_SZ_CAP && reuseChangeOutCount) {
			totalExtraChange -= reuseChangeAmnt;
			--reuseChangeOutCount;
			
			size -= TX_OUTPUT_SZ;
			fee = feePerByte * size;	

			size_t i;
			json_t *input;
			json_array_foreach(inputParams, i, input) {
				// skip removal of first input if forced input set
				if (isLast && rp->forceInputUtxoLast && i==0) { continue; }

				if (totalAmnt - inputAmnts[i] >= totalExtraChange + fee) {
					json_array_set_new(inputParams, i, json_null());
					removedIns = true;

					totalAmnt -= inputAmnts[i];
					inputAmnts[i] = 0;
					usedUnspents[inputIndices[i]] = false;
					size -= TX_INPUT_SZ;
					fee = feePerByte * size;
				}
			}

			changeAmnt = totalAmnt - (totalExtraChange + fee); changeAmnt = changeAmnt > 0 ? changeAmnt : 0;
			if (changeAmnt < feePerByte*(TX_INPUT_SZ)) { fee = feePerByte*(size-TX_OUTPUT_SZ); changeLost = changeAmnt; changeAmnt = 0; }
		}
		if (removedIns) {
			json_t *input;
			size_t i;
			json_array_foreach(inputParams, i, input) {
				if (json_is_null(input)) { json_array_remove(inputParams, i--); --inputsCount; }
			}
		}
	}

	rp->reservedUtxos = unspents;
	rp->reservedUtxosCount = numUnspents - inputsCount;	
	
	if (rp->justCounting) {
		// skip unnecessary RPC calls when just counting
		json_decref(inputParams);
		if (changeAmnt <= TX_DUST_AMNT && changeLost == 0) { changeLost = changeAmnt; }
		goto nosend;
	}

	if ((outputParams = json_object()) == NULL) {
		perror("json_object() failed");
		json_decref(inputParams);
		return CW_SYS_ERR;
	}	

	// construct output for data 
	json_object_set_new(outputParams, "data", json_string(hexDatasC > 1 ? txHexData : *hexDatas));

	if (tinyChange) {
		char tinyChangeAmntStr[B_AMNT_STR_BUF];
		if (snprintf(tinyChangeAmntStr, B_AMNT_STR_BUF, "%.8f", TX_TINYCHANGE_AMNT) >= B_AMNT_STR_BUF) {
			fprintf(CWS_err_stream, "B_AMNT_STR_BUF not set high enough; problem with cashsendtools\n");
			json_decref(inputParams);
			json_decref(outputParams);
			return CW_SYS_ERR;
		}

		const char *tinyAddr;
		if (rp->forceOutputAddrLast) { tinyAddr = rp->forceOutputAddrLast; }
		else {
			// get change address and copy to memory
			if ((status = rpcCall(rp, RPC_M_GETRAWCHANGEADDRESS, NULL, &jsonResult)) != CW_OK) {
				json_decref(inputParams);
				json_decref(outputParams);
				if (jsonResult) { json_decref(jsonResult); }
				return status;
			}
			tinyAddr = json_string_value(jsonResult);
		}	

		json_object_set_new(outputParams, tinyAddr, json_string(tinyChangeAmntStr));

		if (jsonResult) { json_decref(jsonResult); jsonResult = NULL; }
	}

	// create reuse change amount string
	char reuseChangeAmntStr[B_AMNT_STR_BUF];
	if (snprintf(reuseChangeAmntStr, B_AMNT_STR_BUF, "%.8f", reuseChangeAmnt) >= B_AMNT_STR_BUF) {
		fprintf(CWS_err_stream, "B_AMNT_STR_BUF not set high enough; problem with cashsendtools\n");
		json_decref(inputParams);
		json_decref(outputParams);
		return CW_SYS_ERR;
	}

	// construct output(s) for reuse change
	for (int i=0; i<reuseChangeOutCount; i++) {
		// get change address(es) and copy to memory
		if ((status = rpcCall(rp, RPC_M_GETRAWCHANGEADDRESS, NULL, &jsonResult)) != CW_OK) {
			json_decref(inputParams);
			json_decref(outputParams);
			if (jsonResult) { json_decref(jsonResult); }
			return status;
		}

		json_object_set_new(outputParams, json_string_value(jsonResult), json_string(reuseChangeAmntStr));

		if (jsonResult) { json_decref(jsonResult); jsonResult = NULL; }
	}	

	// construct output for change (if more than dust)	
	if (changeAmnt > TX_DUST_AMNT) {		
		// create change amount string
		char changeAmntStr[B_AMNT_STR_BUF];
		if (snprintf(changeAmntStr, B_AMNT_STR_BUF, "%.8f", changeAmnt) >= B_AMNT_STR_BUF) {
			fprintf(CWS_err_stream, "B_AMNT_STR_BUF not set high enough; problem with cashsendtools\n");
			json_decref(inputParams);
			json_decref(outputParams);
			return CW_SYS_ERR;
		}

		// get change address and copy to memory
		if ((status = rpcCall(rp, RPC_M_GETRAWCHANGEADDRESS, NULL, &jsonResult)) != CW_OK) {
			json_decref(inputParams);
			json_decref(outputParams);
			if (jsonResult) { json_decref(jsonResult); }
			return status;
		}

		json_object_set_new(outputParams, json_string_value(jsonResult), json_string(changeAmntStr));

		if (jsonResult) { json_decref(jsonResult); jsonResult = NULL; }
	} else if (changeLost == 0) { changeLost = changeAmnt; }

	// construct params from inputs and outputs
	if ((params = json_array()) == NULL) {
		perror("json_array() failed");
		json_decref(inputParams);
		json_decref(outputParams);
		return CW_SYS_ERR;
	}
	json_array_append(params, inputParams);
	json_decref(inputParams);
	json_array_append(params, outputParams);
	json_decref(outputParams);

	// create raw transaction from params
	char *rawTx;
	status = rpcCall(rp, RPC_M_CREATERAWTRANSACTION, params, &jsonResult);
	json_decref(params);
	if (status != CW_OK) {
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	if ((rawTx = strdup(json_string_value(jsonResult))) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; }
	json_decref(jsonResult);
	jsonResult = NULL;
	if (status != CW_OK) { return status; }

	// edit raw transaction for extra hex datas if present
	if (hexDatasC > 1) {
		if (++txDataSz > 255) {
			fprintf(CWS_err_stream, "collective hex datas too big; sendTxAttempt() may need update in cashsendtools if the standard has changed\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		uint8_t txDataSzByte[1] = { (uint8_t)txDataSz };
		char txDataSzHex[2];
		byteArrToHexStr((const char *)txDataSzByte, 1, txDataSzHex);
		char *rtEditPtr; char *rtEditPtrS;
		if ((rtEditPtr = rtEditPtrS = strstr(rawTx, txHexData)) == NULL) {
			fprintf(CWS_err_stream, "rawTx parsing error in sendTxAttempt(), attached hex data not found; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		bool opRetFound = false;
		for (; !(opRetFound = !strncmp(rtEditPtr, TX_OP_RETURN, 2)) && rtEditPtr-rawTx > 0; rtEditPtr -= 2);
		if (!opRetFound) {
			fprintf(CWS_err_stream, "rawTx parsing error in sendTxAttempt(), op return code not found; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		if ((rtEditPtr -= 2)-rawTx <= 0) {
			fprintf(CWS_err_stream, "rawTx parsing error in sendTxAttempt(), parsing rawTx arrived at invalid location; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		rtEditPtr[0] = txDataSzHex[0]; rtEditPtr[1] = txDataSzHex[1];

		rtEditPtr += 2 + 2;
		int removed = rtEditPtrS - rtEditPtr;
		if (removed < 0) {
			fprintf(CWS_err_stream, "rawTx parsing error in sendTxAttempt(), editing pointer locations wrong; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		int initLen = strlen(rawTx);
		char temp[strlen(rtEditPtrS)+1]; temp[0] = 0;
		strcat(temp, rtEditPtrS);
		strcpy(rtEditPtr, temp); rawTx[initLen-removed] = 0;
	}

	// construct params for signed tx from raw tx
	if ((params = json_array()) == NULL) { perror("json_array() failed"); free(rawTx); return CW_SYS_ERR; }
	json_array_append_new(params, json_string(rawTx));

	// sign the raw transaction
	char *signedTx;
	status = rpcCall(rp, RPC_M_SIGNRAWTRANSACTIONWITHWALLET, params, &jsonResult);
	json_decref(params);
	if (status == CWS_RPC_NO) {
		json_decref(jsonResult);
		fprintf(CWS_err_stream, "error occurred in signing raw transaction; problem with cashsendtools\n\nraw tx:\n%s\n\n",
			rawTx);
		free(rawTx);
		return status;
	} else if (status != CW_OK) {
		free(rawTx);
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	free(rawTx);
	if ((signedTx = strdup(json_string_value(json_object_get(jsonResult, "hex")))) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; }
	json_decref(jsonResult);
	jsonResult = NULL;
	if (status != CW_OK) { return status; }

	// construct params for sending the transaction from signed tx
	if ((params = json_array()) == NULL) { perror("json_array() failed"); free(signedTx); return CW_SYS_ERR; }
	json_array_append_new(params, json_string(signedTx));
	free(signedTx);

	// send transaction and handle potential errors
	status = rpcCall(rp, RPC_M_SENDRAWTRANSACTION, params, &jsonResult);
	json_decref(params);

	if (status == CW_OK) {
		rp->costCount += fee + changeLost;
		strncpy(resTxid, json_string_value(jsonResult), CW_TXID_CHARS);
		resTxid[CW_TXID_CHARS] = 0;
		fprintf(CWS_err_stream, "-");
	} else if (status == CWS_RPC_NO) {
		const char *msg = json_string_value(json_object_get(jsonResult, "message"));
		if (strstr(msg, "too-long-mempool-chain")) { status = CWS_CONFIRMS_NO; }
		else if (strstr(msg, "insufficient priority")) { status = CWS_FEE_NO; }
		else if (strstr(msg, "txn-mempool-conflict") || strstr(msg, "Missing inputs")) { status = CWS_INPUTS_NO; }
		else { fprintf(CWS_err_stream, "\nunhandled RPC error on sendrawtransaction\n"); }
	} else {
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	json_decref(jsonResult);
	jsonResult = NULL;

	nosend:
	if (rp->justCounting) { rp->costCount += fee + changeLost; memset(resTxid, 'F', CW_TXID_CHARS); resTxid[CW_TXID_CHARS] = 0; }
	if (rp->reservedUtxos == unspents && (status == CW_OK || status == CWS_INPUTS_NO)) {
		for (int i=0; i<rp->reservedUtxosCount; i++) {
			if (usedUnspents[i]) { json_array_set_new(rp->reservedUtxos, i, json_null()); }	
		}	
		json_t *utxo;
		size_t i;
		json_array_foreach(rp->reservedUtxos, i, utxo) {
			if (json_is_null(utxo)) { json_array_remove(rp->reservedUtxos, i--); --rp->reservedUtxosCount; }
		}
	}
	if (status == CW_OK) {
		if (rp->txsToSend > 0) { --rp->txsToSend; }

		json_t *reuseUtxo;
		for (int i=0; i<reuseChangeOutCount; i++) {
			if ((reuseUtxo = json_object()) == NULL) { perror("json_object() failed"); return CW_SYS_ERR; }
			json_object_set_new(reuseUtxo, "txid", json_string(resTxid));
			json_object_set_new(reuseUtxo, "vout", json_integer(i+1));
			json_object_set_new(reuseUtxo, "amount", json_real(reuseChangeAmnt));
			json_array_append(rp->reservedUtxos, reuseUtxo);
			json_decref(reuseUtxo);
			++rp->reservedUtxosCount;
		}

		if (distributed && rp->reservedUtxosCount >= rp->txsToSend && rp->txsToSend >= TX_MAX_0CONF_CHAIN && !rp->justCounting) {
			fprintf(CWS_log_stream, "Waiting on 1-conf...");
			int confs;
			json_t *params;
			if ((params = json_array()) == NULL) { perror("json_array() failed"); return CW_SYS_ERR; }
			json_array_append_new(params, json_string(resTxid));
			json_array_append_new(params, json_boolean(true));
			do {
				if ((status = rpcCall(rp, RPC_M_GETRAWTRANSACTION, params, &jsonResult)) != CW_OK) {
					if (jsonResult) { json_decref(jsonResult); }
					json_decref(params);
					return status;
				}
				confs = json_integer_value(json_object_get(jsonResult, "confirmations"));
				json_decref(jsonResult);
				jsonResult = NULL;
			} while (confs < 1);
			json_decref(params);
		}
	}

	return status;
}

static CW_STATUS sendTx(const char **hexDatas, size_t hexDatasC, bool isLast, struct CWS_rpc_pack *rp, char *resTxid) {	
	++rp->txCount;
	if (rp->justTxCounting) { memset(resTxid, 'F', CW_TXID_CHARS); resTxid[CW_TXID_CHARS] = 0; return CW_OK; }

	bool printed = false;
	int count = 0; 
	double balance;
	double balanceN;
	CW_STATUS checkBalStatus;
	CW_STATUS status;
	do { 
		status = sendTxAttempt(hexDatas, hexDatasC, isLast, rp, true, true, resTxid);
		switch (status) {
			case CW_OK:
				break;	
			case CWS_FUNDS_NO:
				if ((checkBalStatus = checkBalance(rp, &balance)) != CW_OK) { return checkBalStatus; }
				do {
					if (!printed) { fprintf(CWS_log_stream, "Insufficient balance, send more funds..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					if ((checkBalStatus = checkBalance(rp, &balanceN)) != CW_OK) { return checkBalStatus; }
				} while (balanceN <= balance);
				break;
			case CWS_CONFIRMS_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, isLast, rp, false, true, resTxid)) == CWS_CONFIRMS_NO) {
					if (!printed) { fprintf(CWS_log_stream, "Waiting on confirmations..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
				}
				break;
			case CWS_FEE_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, isLast, rp, true, false, resTxid)) == CWS_FEE_NO) {
					if (!printed) { fprintf(CWS_log_stream, "Fee problem, attempting to resolve..."); printed = true; }
				}
				break;
			case CWS_INPUTS_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, isLast, rp, true, true, resTxid)) == CWS_INPUTS_NO) {
					if (isLast && rp->forceInputUtxoLast) { break; }
					if (!printed) { fprintf(CWS_log_stream, "Bad UTXOs, attempting to resolve..."); printed = true; }
					if (rp->reservedUtxos && json_array_size(rp->reservedUtxos) == 0 && ++count >= 2) { break; }
				}
				if (status == CWS_INPUTS_NO) { return status; }
				break;
			case CWS_RPC_NO:
				fprintf(CWS_err_stream, "RPC response error: %s\n", rp->errMsg);
				return status;
			default:
				return status;
		}
	} while (status != CW_OK);

	return status;
}

static CW_STATUS hexAppendMetadata(struct CW_file_metadata *md, char *hexStr) {	
	char chainLenHex[CW_MD_CHARS(length)+1]; char treeDepthHex[CW_MD_CHARS(depth)+1];
	char fTypeHex[CW_MD_CHARS(type)+1]; char pVerHex[CW_MD_CHARS(pVer)+1];

	if (!intToNetHexStr(&md->length, CW_MD_BYTES(length), chainLenHex) ||
	    !intToNetHexStr(&md->depth, CW_MD_BYTES(depth), treeDepthHex) ||
	    !intToNetHexStr(&md->type, CW_MD_BYTES(type), fTypeHex) ||
	    !intToNetHexStr(&md->pVer, CW_MD_BYTES(pVer), pVerHex)) { return CW_SYS_ERR; }

	strcat(hexStr, chainLenHex);
	strcat(hexStr, treeDepthHex);
	strcat(hexStr, fTypeHex);
	strcat(hexStr, pVerHex);
	return CW_OK;
}

static bool saveRecoveryData(FILE *recoveryData, int savedTreeDepth, struct CWS_params *params) {
	bool err = false;

	// write recovery info to formatted string
	struct DynamicMemory info;
	initDynamicMemory(&info);
	resizeDynamicMemory(&info, RECOVERY_INFO_BUF);
	if (info.data == NULL) { err = true; goto cleanup; }
	int infoChars;
	while ((infoChars = snprintf(info.data, info.size, "%u\n%u\n%d\n", params->cwType, params->maxTreeDepth, savedTreeDepth)) >= info.size) {
		resizeDynamicMemory(&info, infoChars+1);
		if (info.data == NULL) { err = true; goto cleanup; }
	}

	// this should not happen, but checks for NULL recovery stream just in case
	if (!params->recoveryStream) { fprintf(CWS_err_stream, "ERROR: params->recoveryStream is NULL pointer\n"); err = true; goto cleanup; }

	// write recovery info string to recovery stream	
	if (fputs(info.data, params->recoveryStream) == EOF) { perror("fputs() failed"); err = true; goto cleanup; }

	// write recovery data to recovery stream
	rewind(recoveryData);
	if (copyStreamData(params->recoveryStream, recoveryData) != COPY_OK) { err = true; goto cleanup; }

	cleanup:
		freeDynamicMemory(&info);
		if (err) { fprintf(CWS_err_stream, "ERROR: failed to write recovery data\n"); }
		return !err;
}

static CW_STATUS sendFileChain(FILE *fp, char *pdHexStr, struct CWS_rpc_pack *rp, CW_TYPE cwType, int treeDepth, char *resTxid) { 
	CW_STATUS status;
	char hexChunk[CW_TX_DATA_CHARS + 1]; const char *hexChunkPtr = hexChunk;
	char txid[CW_TXID_CHARS+1]; txid[0] = 0;
	int dataLen = treeDepth ? CW_TX_DATA_CHARS : CW_TX_DATA_BYTES;
	char buf[dataLen];
	int pushdataLen = 0;
	if (pdHexStr) { pushdataLen = treeDepth ? HEX_CHARS(txDataSize(strlen(pdHexStr), NULL)) : txDataSize(strlen(pdHexStr), NULL); }
	int metadataLen = (treeDepth ? CW_METADATA_CHARS : CW_METADATA_BYTES) + pushdataLen;
	if (metadataLen > dataLen) { return CW_CALL_NO; }
	int readSz = treeDepth ? 2 : 1;
	long size = fileSize(fileno(fp));
	int toRead = size <= sizeof(buf) ? size : sizeof(buf);
	int read;
	bool begin = true; bool end = size+metadataLen <= sizeof(buf);
	struct CW_file_metadata md;
	init_CW_file_metadata(&md, cwType);
	md.depth = treeDepth;
	int loc = 0;

	if (fseek(fp, -toRead, SEEK_END) != 0) { perror("fseek() SEEK_END failed"); return CW_SYS_ERR; }
	while (!ferror(fp)) {
		if ((read = fread(buf, readSz, toRead/readSz, fp)*readSz) > 0) {
			if (!treeDepth) {
				byteArrToHexStr(buf, read, hexChunk);
			} else {
				memcpy(hexChunk, buf, read);
				hexChunk[read] = 0;
			}
		} else { hexChunk[0] = 0; }
		strcat(hexChunk, txid);
		if (end) {
			if ((status = hexAppendMetadata(&md, hexChunk)) != CW_OK) { return status; }
			if (pdHexStr) { 
				const char *hexDatas[2] = { hexChunkPtr, pdHexStr };
				if ((status = sendTx(hexDatas, sizeof(hexDatas)/sizeof(hexDatas[0]), end, rp, txid)) != CW_OK) { return status; }
				break;
			}
		}
		if ((status = sendTx(&hexChunkPtr, 1, end, rp, txid)) != CW_OK) { return status; }
		if (end) { break; }
		++md.length;
		if (begin) { toRead -= treeDepth ? CW_TXID_CHARS : CW_TXID_BYTES; begin = false; }
		if ((loc = ftell(fp))-read < toRead) {
			if (loc-read < toRead-metadataLen) { end = true; }
			toRead = loc-read;
			if (fseek(fp, 0, SEEK_SET) != 0) { perror("fseek() SEEK_SET failed"); return CW_SYS_ERR; }
		} else if (fseek(fp, -read-toRead, SEEK_CUR) != 0) { perror("fseek() SEEK_CUR failed"); return CW_SYS_ERR; }
	}
	if (ferror(fp)) { perror("file error on fread()"); return CW_SYS_ERR; }

	strncpy(resTxid, txid, CW_TXID_CHARS); resTxid[CW_TXID_CHARS] = 0;
	return CW_OK;
}

static CW_STATUS sendFileTreeLayer(FILE *fp, char *pdHexStr, struct CWS_rpc_pack *rp, CW_TYPE cwType, int depth, int *numTxs, FILE *treeFp) {
	CW_STATUS status;
	char hexChunk[CW_TX_DATA_CHARS + 1]; const char *hexChunkPtr = hexChunk;
	char txid[CW_TXID_CHARS+1]; txid[0] = 0;
	int pushdataLen = 0;
	if (pdHexStr) { pushdataLen = depth ? HEX_CHARS(txDataSize(strlen(pdHexStr), NULL)) : txDataSize(strlen(pdHexStr), NULL); }
	int metadataLen = (depth ? CW_METADATA_CHARS : CW_METADATA_BYTES) + pushdataLen;
	bool atMetadata = false;
	int dataLen = depth ? CW_TX_DATA_CHARS : CW_TX_DATA_BYTES;
	if (metadataLen > dataLen) { return CW_CALL_NO; }
	char buf[dataLen];
	*numTxs = 0;
	bool rootCheck = false;
	int readSz = depth ? 2 : 1;

	int n;
	while ((n = fread(buf, readSz, sizeof(buf)/readSz, fp)*readSz) > 0 || !rootCheck) {
		if (n > 0) {
			if (!depth) {
				byteArrToHexStr(buf, n, hexChunk);
			} else { 
				memcpy(hexChunk, buf, n); 
				hexChunk[n] = 0; 
			}
		} else { ++*numTxs; break; }

		if (feof(fp) && *numTxs < 1) {
			rootCheck = true;
			if (n+metadataLen <= dataLen) {
				struct CW_file_metadata md;
				init_CW_file_metadata(&md, cwType);
				md.depth = depth;
				if ((status = hexAppendMetadata(&md, hexChunk)) != CW_OK) { return status; }
				atMetadata = true;
			} else { ++*numTxs; }
		}

		if (atMetadata && pdHexStr) {
			const char *hexDatas[2] = { hexChunkPtr, pdHexStr };
			if ((status = sendTx(hexDatas, sizeof(hexDatas)/sizeof(hexDatas[0]), atMetadata, rp, txid)) != CW_OK) { return status; }
		} else {
			if ((status = sendTx(&hexChunkPtr, 1, atMetadata, rp, txid)) != CW_OK) { return status; }
		}
		++*numTxs;	
		if (fputs(txid, treeFp) == EOF) { perror("fputs() failed"); return CW_SYS_ERR; }
	}
	if (ferror(fp)) { perror("file error on fread()"); return CW_SYS_ERR; }

	return CW_OK;
}

static CW_STATUS sendFileTree(FILE *fp, char *pdHexStr, struct CWS_rpc_pack *rp, struct CWS_params *params, int depth, double *fundsLost, char *resTxid) {
	CW_STATUS status;
	double safeCost = rp->costCount;
	bool recover = false;

	FILE *tfp = NULL;

	if (params->maxTreeDepth >= 0 && depth >= params->maxTreeDepth) {
		if ((status = sendFileChain(fp, pdHexStr, rp, params->cwType, depth, resTxid)) != CW_OK) { recover = true; }
		goto cleanup;
	}

	if ((tfp = tmpfile()) == NULL) { perror("tmpfile() failed"); return CW_SYS_ERR; }

	int numTxs;
	if ((status = sendFileTreeLayer(fp, pdHexStr, rp, params->cwType, depth, &numTxs, tfp)) != CW_OK) { recover = true; goto cleanup; }
	rewind(tfp);

	if (numTxs < 2) {
		if (fgets(resTxid, CW_TXID_CHARS+1, tfp) == NULL && ferror(tfp)) {
			perror("fgets() failed in sendFileTree()");
			recover = true;
			status = CW_SYS_ERR;
			goto cleanup;
		}
		status = CW_OK;
		goto cleanup;
	}

	status = sendFileTree(tfp, pdHexStr, rp, params, depth+1, fundsLost, resTxid);

	cleanup:
		if (tfp) { fclose(tfp); }
		if (status != CW_OK && status != CW_CALL_NO && recover) {
			if (fundsLost) { *fundsLost = rp->costCount; }
			if (depth > 0) {
				fprintf(CWS_log_stream, "\nError met, saving recovery data...\n");
				if (saveRecoveryData(fp, depth, params)) {
					fprintf(CWS_log_stream, "Recovery data saved!\n");
					if (fundsLost) { *fundsLost -= safeCost; }
				} else { fprintf(CWS_log_stream, "Failed to save recovery data; progress lost\n"); }
			}
		}
		return status;
}

static CW_STATUS sendFileFromStream(FILE *stream, int sDepth, char *pdHexStr, struct CWS_rpc_pack *rp, struct CWS_params *params, double *fundsLost, char *resTxid) {
	if (params->cwType == CW_T_MIMESET && !rp->justCounting) {
		fprintf(CWS_err_stream, "WARNING: params specified type CW_T_MIMESET, but cashsendtools cannot determine mimetype when sending from stream;\n"
				 "defaulting to CW_T_FILE\n");
		params->cwType = CW_T_FILE;
	}
	CW_STATUS status;

	status = sendFileTree(stream, pdHexStr, rp, params, sDepth, fundsLost, resTxid);

	if (!rp->justCounting) { fprintf(CWS_log_stream, "\n"); }
	return status;
}

static CW_STATUS sendFileFromPath(const char *path, struct CWS_rpc_pack *rp, struct CWS_params *params, double *fundsLost, char *resTxid) {
	FILE *fp;
	if ((fp = fopen(path, "rb")) == NULL) { perror("fopen() failed"); return CW_SYS_ERR; }	

	CW_STATUS status;	

	if (params->cwType == CW_T_MIMESET) {
		if ((status = CWS_set_cw_mime_type_by_extension(path, params)) != CW_OK) { goto cleanup; }
		if (!rp->justCounting && params->cwType == CW_T_FILE) {
			fprintf(CWS_err_stream, "\ncashsendtools failed to match '%s' to anything in mime.types; defaults to CW_T_FILE\n", path);
		}
	}

	status = sendFileFromStream(fp, 0, NULL, rp, params, fundsLost, resTxid);

	cleanup:
		fclose(fp);
		return status;
}

static CW_STATUS scanDirFileCount(char const *ftsPathArg[], int *count) {
	FTS *ftsp;
	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	if ((ftsp = fts_open((char * const *)ftsPathArg, fts_options, NULL)) == NULL) { perror("fts_open() failed"); return CW_SYS_ERR; }

	*count = 0;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL) { if (p->fts_info == FTS_F) { ++*count; } }

	fts_close(ftsp);
	return CW_OK;
}

static CW_STATUS sendDirFromPath(const char *path, struct CWS_rpc_pack *rp, struct CWS_params *params, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	// get directory file count in advance, for memory allocation purposes
	char const *ftsPathArg[] = { path, NULL };
	int numFiles;
	if ((status = scanDirFileCount(ftsPathArg, &numFiles)) != CW_OK) { return status; }

	// initializing variable-length array before goto statements
	char txidsByteData[numFiles*CW_TXID_BYTES];

	// initializing these here in case of failure
	FTS *ftsp = NULL;
	FILE *dirFp = NULL;

	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	if ((ftsp = fts_open((char * const *)ftsPathArg, fts_options, NULL)) == NULL) {
		perror("fts_open() failed");
		status = CW_SYS_ERR;
		goto cleanup;
	}
	int pathLen = strlen(path);

	if (!rp->justCounting) {
		if (params->saveDirStream) { dirFp = params->saveDirStream; }
		else if (!params->dirOmitIndex && (dirFp = tmpfile()) == NULL) {
			perror("tmpfile() failed");
			status = CW_SYS_ERR;
			goto cleanup;
		}
	}

	char txid[CW_TXID_CHARS+1];
	struct CWS_params fileParams;

	// find and send all files in directory
	int count = 0;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL && count < numFiles) {
		if (p->fts_info == FTS_F && strncmp(p->fts_path, path, pathLen) == 0) {
			if (!rp->justCounting) { fprintf(CWS_log_stream, "Sending %s...", p->fts_path+pathLen); }

			// create a clean copy of params for every file, as they may be altered during send
			copy_CWS_params(&fileParams, params);

			if ((status = sendFileFromPath(p->fts_path, rp, &fileParams, fundsLost, txid)) != CW_OK) { goto cleanup; }
			if (!rp->justCounting) { fprintf(CWS_log_stream, "%s\n\n", txid); }

			if (dirFp) {
				// txids are stored as byte data (rather than hex string) in txidsByteData
				if (hexStrToByteArr(txid, 0, txidsByteData+(count*CW_TXID_BYTES)) != CW_TXID_BYTES) {
					fprintf(CWS_log_stream, "invalid txid from sendFile(); problem with cashsendtools\n");
					status = CW_SYS_ERR;
					goto cleanup;
				}

				// write file path to directory index
				if (fprintf(dirFp, "%s\n", p->fts_path+pathLen) < 0) {
					perror("fprintf() to dirFp failed");
					status = CW_SYS_ERR;
					goto cleanup;
				}
			}
			++count;
		}
	}
	if (dirFp) {
		// necessary empty line between path information and txid byte data
		if (fprintf(dirFp, "\n") < 0) { perror("fprintf() to dirFp failed"); status = CW_SYS_ERR; goto cleanup; }
		// write txid byte data to directory index
		if (fwrite(txidsByteData, CW_TXID_BYTES, numFiles, dirFp) < numFiles) { perror("fwrite() to dirFp failed");
											   status = CW_SYS_ERR; goto cleanup; }

		if (!params->dirOmitIndex) {
			// send directory index
			rewind(dirFp);
			if (!rp->justCounting) { fprintf(CWS_log_stream, "%s directory index...", dirFp == params->saveDirStream ? "Saving" : "Sending"); }
			struct CWS_params dirIndexParams;
			copy_CWS_params(&dirIndexParams, params);
			dirIndexParams.cwType = CW_T_DIR;
			if ((status = sendFileFromStream(dirFp, 0, NULL, rp, &dirIndexParams, fundsLost, resTxid)) != CW_OK) { goto cleanup; }
		} else { resTxid[0] = 0; }
	}

	cleanup:
		if (dirFp && dirFp != params->saveDirStream) { fclose(dirFp); }
		if (ftsp) { fts_close(ftsp); }
		return status;
}

static inline CW_STATUS sendFromPath(const char *path, bool asDir, struct CWS_rpc_pack *rp, struct CWS_params *csp, double *fundsLost, char *resTxid) {
	CW_STATUS (*send) (const char *, struct CWS_rpc_pack *, struct CWS_params *, double *, char *) = asDir ? &sendDirFromPath : &sendFileFromPath;
	return send(path, rp, csp, fundsLost, resTxid);
}

static CW_STATUS initRpc(struct CWS_params *params, struct CWS_rpc_pack *rpcPack) {
	// initialize bitcoin RPC client
	bitcoinrpc_global_init();
	rpcPack->cli = bitcoinrpc_cl_init_params(params->rpcUser, params->rpcPass, params->rpcServer, params->rpcPort);
	if (rpcPack->cli == NULL) { perror("bitcoinrpc_cl_init_params() failed"); return CWS_RPC_ERR;  }	

	// initialize RPC methods to be used
	BITCOINRPC_METHOD method;
	json_t *mParams;
	char *nonStdName;
	for (int i=0; i<RPC_METHODS_COUNT; i++) {
		rpcPack->methods[i] = NULL;
		mParams = NULL;
		nonStdName = NULL;
		switch (i) {
			case RPC_M_GETBALANCE:
				method = BITCOINRPC_METHOD_GETBALANCE;
				break;
			case RPC_M_GETUNCONFIRMEDBALANCE:
				method = BITCOINRPC_METHOD_GETUNCONFIRMEDBALANCE;
				break;
			case RPC_M_LISTUNSPENT:
				method = BITCOINRPC_METHOD_LISTUNSPENT;
				break;
			case RPC_M_LISTUNSPENT_0:
				method = BITCOINRPC_METHOD_LISTUNSPENT;
				if ((mParams = json_array()) == NULL) { perror("json_array() failed"); return CW_SYS_ERR; }
				json_array_append_new(mParams, json_integer(0));
				break;
			case RPC_M_GETRAWCHANGEADDRESS:
				method = BITCOINRPC_METHOD_GETRAWCHANGEADDRESS;
				break;
			case RPC_M_ESTIMATEFEE:
				method = BITCOINRPC_METHOD_ESTIMATEFEE;
				break;
			case RPC_M_CREATERAWTRANSACTION:
				method = BITCOINRPC_METHOD_CREATERAWTRANSACTION;
				break;
			case RPC_M_SIGNRAWTRANSACTIONWITHWALLET:
				method = BITCOINRPC_METHOD_NONSTANDARD;
				nonStdName = "signrawtransactionwithwallet";
				break;
			case RPC_M_SENDRAWTRANSACTION:
				method = BITCOINRPC_METHOD_SENDRAWTRANSACTION;
				break;
			case RPC_M_GETRAWTRANSACTION:
				method = BITCOINRPC_METHOD_GETRAWTRANSACTION;
				break;
			case RPC_M_LOCKUNSPENT:
				method = BITCOINRPC_METHOD_LOCKUNSPENT;
				break;
			default:
				continue;
		}

		rpcPack->methods[i] = bitcoinrpc_method_init_params(method, mParams);
		if (mParams != NULL) { json_decref(mParams); }
		if (rpcPack->methods[i] == NULL) {
			fprintf(CWS_err_stream, "bitcoinrpc_method_init() failed for rpc method %d: %s\n", i, strerror(errno));
			return CW_SYS_ERR;
		}
		if (nonStdName != NULL) { bitcoinrpc_method_set_nonstandard(rpcPack->methods[i], nonStdName); }
	}	

	// initialize all rpcPack variables
	rpcPack->txsToSend = 0;
	rpcPack->reservedUtxos = NULL;
	rpcPack->reservedUtxosCount = 0;
	rpcPack->forceTinyChangeLast = false;
	rpcPack->forceInputUtxoLast = NULL;
	rpcPack->forceOutputAddrLast = params->revToAddr;
	rpcPack->costCount = 0;
	rpcPack->txCount = 0;
	rpcPack->errMsg[0] = 0;
	rpcPack->justCounting = false;
	rpcPack->justTxCounting = false;

	// construct path to revision locks from params
	rpcPack->revLocksPath = NULL;
	struct stat st;
	if (stat(params->datadir, &st) == -1) { perror("stat() failed on data directory"); return CW_SYS_ERR; }
	if (!S_ISDIR(st.st_mode)) { return CW_DATADIR_NO; }
	size_t dataDirPathLen = strlen(params->datadir);
	bool appendSlash = params->datadir[dataDirPathLen-1] != '/';	
	size_t revLocksPathSz = dataDirPathLen+1+strlen(CW_DATADIR_REVISIONS_FILE)+1;
	if ((rpcPack->revLocksPath = malloc(revLocksPathSz)) == NULL) { perror("malloc failed"); return CW_SYS_ERR; }
	snprintf(rpcPack->revLocksPath, revLocksPathSz, "%s%s%s", params->datadir, appendSlash ? "/" : "", CW_DATADIR_REVISIONS_FILE);

	// load revision locks and ensure locked unspents
	rpcPack->revisionLocks = NULL;
	if (access(rpcPack->revLocksPath, F_OK) == 0) {
		json_error_t e;
		if ((rpcPack->revisionLocks = json_load_file(rpcPack->revLocksPath, 0, &e)) == NULL) {
			fprintf(CWS_err_stream, "json_load_file() failed\nMessage:%s\n", e.text);
			return CW_SYS_ERR;
		}	
		CW_STATUS lockStatus = lockRevisionUnspents(rpcPack->revisionLocks, false, rpcPack);	
		if (lockStatus != CW_OK) { return lockStatus; }
	} else if ((rpcPack->revisionLocks = json_array()) == NULL) { perror("json_array() failed"); return CW_SYS_ERR; }	

	return CW_OK;
}

static void cleanupRpc(struct CWS_rpc_pack *rpcPack) {
	if (rpcPack->cli) {
		bitcoinrpc_cl_free(rpcPack->cli);
		for (int i=0; i<RPC_METHODS_COUNT; i++) { if (rpcPack->methods[i]) { bitcoinrpc_method_free(rpcPack->methods[i]); } }
	}
	if (rpcPack->reservedUtxos) { json_decref(rpcPack->reservedUtxos); }
	if (rpcPack->revisionLocks) { json_decref(rpcPack->revisionLocks); }
	if (rpcPack->revLocksPath) { free(rpcPack->revLocksPath); }
	
	bitcoinrpc_global_cleanup();
}

static inline void init_CWS_sender_for_stream(struct CWS_sender *css, FILE *stream, int sDepth, char *pdHexStr, struct CWS_rpc_pack *rp, struct CWS_params *csp) {
	css->fromStream = &sendFileFromStream;	
	css->fromPath = NULL;
	css->stream = stream;
	css->sDepth = sDepth;
	css->pdHexStr = pdHexStr;
	css->path = NULL;
	css->asDir = false;
	css->rp = rp;
	css->csp = csp;
}

static inline void init_CWS_sender_for_path(struct CWS_sender *css, const char *path, bool asDir, struct CWS_rpc_pack *rp, struct CWS_params *csp) {
	css->fromStream = NULL;
	css->fromPath = &sendFromPath;
	css->stream = NULL;
	css->path = path;
	css->pdHexStr = NULL;
	css->asDir = asDir;
	css->rp = rp;
	css->csp = csp;
	css->sDepth = 0;

}

static CW_STATUS countBySender(struct CWS_sender *sender, size_t *txCount, double *costCount) {
	sender->rp->costCount = 0;
	sender->rp->txCount = 0;
	sender->rp->justCounting = true;
	if (!costCount) { sender->rp->justTxCounting = true; }
	char dummyTxid[CW_TXID_CHARS+1] = { 0 };

	CW_STATUS status;
	if (sender->fromStream) {
		status = sender->fromStream(sender->stream, sender->sDepth, sender->pdHexStr, sender->rp, sender->csp, NULL, dummyTxid);
	}
	else if (sender->fromPath) {
		status = sender->fromPath(sender->path, sender->asDir, sender->rp, sender->csp, NULL, dummyTxid);
	}
	else { fprintf(CWS_err_stream, "no send func specified in struct CWS_sender; problem with cashsendtools"); status = CW_SYS_ERR; }

	sender->rp->justTxCounting = false;
	sender->rp->justCounting = false;
	if (txCount) { *txCount = sender->rp->txCount; }
	if (costCount) { *costCount = sender->rp->costCount; }
	sender->rp->txCount = 0;
	sender->rp->costCount = 0;
	return status;
}

static CW_STATUS sendBySender(struct CWS_sender *sender, double *fundsLost, char *resTxid) {
	sender->rp->costCount = 0;
	sender->rp->txCount = 0;	

	CW_STATUS status;
	if (sender->fromStream) {
		status = sender->fromStream(sender->stream, sender->sDepth, sender->pdHexStr, sender->rp, sender->csp, fundsLost, resTxid);
	}
	else if (sender->fromPath) {
		status = sender->fromPath(sender->path, sender->asDir, sender->rp, sender->csp, fundsLost, resTxid);
	}
	else { fprintf(CWS_err_stream, "no send func specified in struct CWS_sender; problem with cashsendtools"); status = CW_SYS_ERR; }

	return status;
}
