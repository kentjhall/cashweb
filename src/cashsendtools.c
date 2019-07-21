#include "cashsendtools.h"
#include "cashwebutils.h"

/* general constants */
#define LINE_BUF 150
#define ERR_WAIT_CYCLE 5
#define ERR_MSG_BUF 40
#define RECOVERY_INFO_BUF 15
#define RECOVERY_DATA_BUF 1000

/* rpc method identifiers */
#define RPC_M_GETBALANCE 0
#define RPC_M_GETUNCONFIRMEDBALANCE 1
#define RPC_M_LISTUNSPENT 2
#define RPC_M_LISTUNSPENT_0 3
#define RPC_M_GETRAWCHANGEADDRESS 4
#define RPC_M_ESTIMATEFEE 5
#define RPC_M_CREATERAWTRANSACTION 6
#define RPC_M_SIGNRAWTRANSACTIONWITHWALLET 7
#define RPC_M_SENDRAWTRANSACTION 8
#define RPC_METHODS_COUNT 9

/* tx sending constants */
#define B_ERR_MSG_BUF 50
#define B_ADDRESS_BUF 75
#define B_AMNT_STR_BUF 32
#define TX_OP_RETURN "6a"
#define TX_OP_PUSHDATA1 "4c"
#define TX_OP_PUSHDATA1_THRESHOLD 75
#define TX_DUST_AMNT 0.00000545
#define TX_BASE_SZ 10
#define TX_INPUT_SZ 148
#define TX_OUTPUT_SZ 34
#define TX_DATA_BASE_SZ 10

/*
 * struct for carrying initialized bitcoinrpc_cl_t and bitcoinrpc_methods
 * cli: RPC client struct
 * methods: array of RPC method structs; contains only methods to be used by cashsendtools
 * unspents: JSON array of wallet's unspent UTXOS
 * costAccrued: tracks how much funds have been lost
 * errMsg: carries last RPC error message
 */
struct CWS_rpc_pack {
	bitcoinrpc_cl_t *cli;
	struct bitcoinrpc_method *methods[RPC_METHODS_COUNT];
	json_t *unspents;
	double costAccrued;
	char errMsg[ERR_MSG_BUF];
};

/*
 * attempts an RPC call via given struct CWS_rpc_pack with specified RPC method identifier and params
 * copies json response pointer to jsonResult; this needs to be freed by json_decref()
 */
static CW_STATUS rpcCallAttempt(struct CWS_rpc_pack *rp, int rpcMethodI, json_t *params, json_t **jsonResult);

/*
 * wrapper for rpcCallAttempt() to wait on connection error
 * will return appropriate status on any other error
 */
static CW_STATUS rpcCall(struct CWS_rpc_pack *rp, int rpcMethodI, json_t *params, json_t **jsonResult);

/*
 * check balance of wallet via RPC and writes to balance 
 */
static CW_STATUS checkBalance(struct CWS_rpc_pack *rp, double *balance);

/*
 * calculates data size in tx for data of length hexDataLen by accounting for extra opcodes
 * writes true/false to extraByte for whether or not an extra byte was needed
 */
static int txDataSize(int hexDataLen, bool *extraByte);

/*
 * attempts to send tx with OP_RETURN data as per hexDatas (can be multiple pushdatas) via RPC
 * specify useUnconfirmed for whether or not 0-conf unspents are to be used
 * writes resultant txid to resTxid
 */
static CW_STATUS sendTxAttempt(const char **hexDatas, int hexDatasC, struct CWS_rpc_pack *rp, bool useUnconfirmed, bool sameFee, char *resTxid);

/*
 * wrapper for sendTxAttempt() to handle errors involving insufficient balance, confirmations, and fees
 * will return appropriate status on any other error
 */
static CW_STATUS sendTx(const char **hexDatas, int hexDatasC, struct CWS_rpc_pack *rp, char *resTxid);

/*
 * puts int to network byte order (big-endian) and converts to hex string, written to passed memory location
 * must make sure that type of uintPtr matches numBytes (e.g. uint16_t -> 2 bytes), and that hexStr has sufficient space
 * supports 2 and 4 byte integers
 */
static CW_STATUS intToNetHexStr(void *uintPtr, int numBytes, char *hexStr);

/*
 * appends struct CW_file_metadata values to given hex string
 */
static void hexAppendMetadata(struct CW_file_metadata *md, char *hexStr);

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
static CW_STATUS sendFileChain(FILE *fp, struct CWS_rpc_pack *rp, CW_TYPE cwType, int treeDepth, char *resTxid);

/*
 * sends layer of file tree (i.e. all TXs at same depth) from stream fp via RPC; writes all txids to treeFp
 * if only one TX is sent, determines that this is root and constructs/appends appropriate file metadata with cwType and depth
 * writes number of TXs sent to numTxs
 */
static CW_STATUS sendFileTreeLayer(FILE *fp, struct CWS_rpc_pack *rp, CW_TYPE cwType, int depth, int *numTxs, FILE *treeFp);

/*
 * sends file from stream fp as tree of TXs via RPC and writes resultant txid to resTxid,
   or in case of failure, writes the amount of funds lost (accounts for saved recovery data, so may amount to less than accrued cost)
 * uses params for passing cwType, handling maxTreeDepth, and saving to recoveryStream in case of failure
 * if maxTreeDepth is reached, will divert to sendFileChain() for sending as a chain of tree root TXs
 */
static CW_STATUS sendFileTree(FILE *fp, struct CWS_rpc_pack *rp, struct CWS_params *params, int depth, double *fundsLost, char *resTxid);

/*
 * sends file from stream (in accordance with params) via RPC and writes resultant txid to resTxid,
   or in case of failure, writes the amount of funds lost (accounts for saved recovery data, so may amount to less than accrued cost)
 * depth can be specified in case of sending from recovery data
 */
static CW_STATUS sendFileFromStream(FILE *stream, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, int depth, double *fundsLost, char *resTxid);

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

/* ------------------------------------- PUBLIC ------------------------------------- */

CW_STATUS CWS_send_from_stream(FILE *stream, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	status = sendFileFromStream(stream, &rpcPack, params, 0, fundsLost, resTxid);	
	
	if (fundsUsed != NULL) { *fundsUsed = rpcPack.costAccrued; }
	cleanupRpc(&rpcPack);
	return status;
}

CW_STATUS CWS_send_from_recovery_stream(FILE *recoveryStream, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	struct DynamicMemory line;
	initDynamicMemory(&line);

	int readlineStatus;
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) { goto cleanup; }
	params->cwType = atoi(line.data);
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) { goto cleanup; }
	params->maxTreeDepth = atoi(line.data);
	if ((readlineStatus = safeReadLine(&line, RECOVERY_INFO_BUF, recoveryStream)) != READLINE_OK) { goto cleanup; }
	int depth = atoi(line.data);

	status = sendFileFromStream(recoveryStream, &rpcPack, params, depth, fundsLost, resTxid);

	cleanup:
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costAccrued; }
		if (ferror(recoveryStream)) { perror("fgets() failed on recovery stream"); status = CW_SYS_ERR; }
		else if (readlineStatus == READLINE_ERR) { status = CW_SYS_ERR; }
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_from_path(const char *path, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	status = sendFileFromPath(path, &rpcPack, params, fundsLost, resTxid);
	
	if (fundsUsed != NULL) { *fundsUsed = rpcPack.costAccrued; }
	cleanupRpc(&rpcPack);
	return status;
}

CW_STATUS CWS_send_dir_from_path(const char *path, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid) {
	CW_STATUS status;

	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	// get directory file count in advance, for memory allocation purposes
	char const *ftsPathArg[] = { path, NULL };
	int numFiles;
	if ((status = scanDirFileCount(ftsPathArg, &numFiles)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	// initializing variable-length array before goto statements
	char txidsByteData[numFiles*CW_TXID_BYTES];

	// initializing these here in case of failure
	FTS *ftsp = NULL;
	FILE *tmpDirFp = NULL;

	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	if ((ftsp = fts_open((char * const *)ftsPathArg, fts_options, NULL)) == NULL) {
		perror("fts_open() failed");
		status = CW_SYS_ERR;
		goto cleanup;
	}
	int pathLen = strlen(path);

	if ((tmpDirFp = tmpfile()) == NULL) {
		perror("tmpfile() failed");
		status = CW_SYS_ERR;
		goto cleanup;
	}

	char txid[CW_TXID_CHARS+1];

	// find and send all files in directory
	int count = 0;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL && count < numFiles) {
		if (p->fts_info == FTS_F && strncmp(p->fts_path, path, pathLen) == 0) {
			fprintf(stderr, "Sending %s", p->fts_path+pathLen);

			// create a clean copy of params for every file, as they may be altered during send
			struct CWS_params fileParams;
			copy_CWS_params(&fileParams, params);

			if ((status = sendFileFromPath(p->fts_path, &rpcPack, &fileParams, fundsLost, txid)) != CW_OK) { goto cleanup; }
			fprintf(stderr, "%s\n\n", txid);

			// txids are stored as byte data (rather than hex string) in txidsByteData
			if (hexStrToByteArr(txid, 0, txidsByteData+(count*CW_TXID_BYTES)) != CW_TXID_BYTES) {
				fprintf(stderr, "invalid txid from sendFile(); problem with cashsendtools\n");
				status = CW_SYS_ERR;
				goto cleanup;
			}
			++count;

			// write file path to directory index
			if (fprintf(tmpDirFp, "%s\n", p->fts_path+pathLen) < 0) {
				perror("fprintf() to tmpDirFp failed");
				status = CW_SYS_ERR;
				goto cleanup;
			}
		}
	}
	// necessary empty line between path information and txid byte data
	if (fprintf(tmpDirFp, "\n") < 0) { perror("fprintf() to tmpDirFp failed"); status = CW_SYS_ERR; goto cleanup; }
	// write txid byte data to directory index
	if (fwrite(txidsByteData, CW_TXID_BYTES, numFiles, tmpDirFp) < numFiles) { perror("fwrite() to tmpDirFp failed");
										   status = CW_SYS_ERR; goto cleanup; }

	// send directory index
	rewind(tmpDirFp);
	fprintf(stderr, "Sending directory index");
	struct CWS_params dirIndexParams;
	copy_CWS_params(&dirIndexParams, params);
	dirIndexParams.cwType = CW_T_DIR;
	if ((status = sendFileFromStream(tmpDirFp, &rpcPack, &dirIndexParams, 0, fundsLost, resTxid)) != CW_OK) { goto cleanup; }

	cleanup:
		if (tmpDirFp) { fclose(tmpDirFp); }
		if (ftsp) { fts_close(ftsp); }
		if (fundsUsed != NULL) { *fundsUsed = rpcPack.costAccrued; }
		cleanupRpc(&rpcPack);
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
		fprintf(stderr, "fopen() failed on path %s; unable to open cashweb mime.types\n", mtFilePath);
		perror(NULL);
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
	if (!matched) {
		fprintf(stderr, "\ncashsendtools failed to match '%s' to anything in mime.types; defaults to CW_T_FILE\n", fname);
		csp->cwType = CW_T_FILE;
	}

	cleanup:
		freeDynamicMemory(&line);
		if (mimeTypes) { fclose(mimeTypes); }
		return status;
}

const char *CWS_errno_to_msg(int errNo) {
	switch (errNo) {
		case CW_DATADIR_NO:
			return "Unable to find proper cashwebtools data directory";
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

static CW_STATUS rpcCallAttempt(struct CWS_rpc_pack *rp, int rpcMethodI, json_t *params, json_t **jsonResult) {
	bitcoinrpc_resp_t *bResponse = bitcoinrpc_resp_init();
	bitcoinrpc_err_t bError;
	if (!bResponse) { perror("bitcoinrpc_resp_init() failed"); return CW_SYS_ERR; }
	CW_STATUS status = CW_OK;

	struct bitcoinrpc_method *rpcMethod = rp->methods[rpcMethodI];
	if (!rpcMethod) { fprintf(stderr, "rpc method (identifier %d) not initialized; probably an issue with cashsendtools\n", rpcMethodI);
			  status = CW_SYS_ERR; goto cleanup; }
	if (params != NULL && bitcoinrpc_method_set_params(rpcMethod, params) != BITCOINRPCE_OK) {
		fprintf(stderr, "count not set params for rpc method (identifier %d); probably an issue with cashsendtools\n", rpcMethodI);
		status = CW_SYS_ERR;
		goto cleanup;
	}
	bitcoinrpc_call(rp->cli, rpcMethod, bResponse, &bError);	

	if (bError.code != BITCOINRPCE_OK) { fprintf(stderr, "rpc error %d [%s]\n", bError.code, bError.msg); status = CWS_RPC_ERR; goto cleanup; }

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

static CW_STATUS rpcCall(struct CWS_rpc_pack *rp, int rpcMethodI, json_t *params, json_t **jsonResult) {
	bool printed = false;
	CW_STATUS status;
	while ((status = rpcCallAttempt(rp, rpcMethodI, params, jsonResult)) == CWS_RPC_ERR) {
		if (!printed) {
			fprintf(stderr, "\nRPC request failed, please ensure bitcoind is running and configured correctly; retrying...\n");
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

static int txDataSize(int hexDataLen, bool *extraByte) {
	int dataSize = hexDataLen/2;
	int added = dataSize > TX_OP_PUSHDATA1_THRESHOLD ? 2 : 1;
	if (extraByte != NULL) { *extraByte = added > 1; }
	return dataSize + added; 
}

static CW_STATUS sendTxAttempt(const char **hexDatas, int hexDatasC, struct CWS_rpc_pack *rp, bool useUnconfirmed, bool sameFee, char *resTxid) {
	CW_STATUS status;
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

	// get unspent utxos (may or may not include unconfirmed) if not yet present in rp;
	// carrying these around in struct CW_rpc_pack for limiting RPC calls, as list of unspents can get rather long
	size_t numUnspents = rp->unspents ? json_array_size(rp->unspents) : 0;
	if (!rp->unspents || numUnspents == 0) {
		if (rp->unspents) { json_decref(rp->unspents); rp->unspents = NULL; }
		if ((status = rpcCall(rp, useUnconfirmed ? RPC_M_LISTUNSPENT_0 : RPC_M_LISTUNSPENT, NULL, &rp->unspents)) != CW_OK) {
			if (rp->unspents) { json_decref(rp->unspents); rp->unspents = NULL; }
			return status;
		}
		if ((numUnspents = json_array_size(rp->unspents)) == 0) { return useUnconfirmed ? CWS_FUNDS_NO : CWS_CONFIRMS_NO; }
	}
	bool usedUnspents[numUnspents]; memset(usedUnspents, 0, numUnspents);

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
				fprintf(stderr, "sendTxAttempt() doesn't support data >255 bytes; cashsendtools may need revision if this standard has changed\n");
				return CW_SYS_ERR;
			}
			else if (extraByte) { strcat(txHexData, TX_OP_PUSHDATA1); }
			*szByte = (uint8_t)dataSz;
			byteArrToHexStr((const char *)szByte, 1, szHex);
			strcat(txHexData, szHex);
			strcat(txHexData, hexDatas[i]);
		}
	}

	int numInputs = 0;
	int size;
	double totalAmnt = 0;
	double fee = 0.00000001; // arbitrary
	double changeAmnt = 0;	
	double changeLost = 0;

	char txid[CW_TXID_CHARS+1];
	int vout;
	if ((inputParams = json_array()) == NULL) { perror("json_array() failed"); return CW_SYS_ERR; }
	
	// iterate through unspent utxos
	for (int i=0; i<numUnspents; i++) {
		// pull utxo data
		utxo = json_array_get(rp->unspents, i);

		// copy txid from utxo data to memory
		txid[0] = 0;
		strncat(txid, json_string_value(json_object_get(utxo, "txid")), CW_TXID_CHARS);

		// copy vout from utxo data to memory
		vout = json_integer_value(json_object_get(utxo, "vout"));

		// construct input from txid and vout
		if ((input = json_object()) == NULL) {
			perror("json_object() failed");
			json_decref(inputParams);
			return CW_SYS_ERR;
		}
		json_object_set_new(input, "txid", json_string(txid));
		json_object_set_new(input, "vout", json_integer(vout));

		// remove used unspent from stored array
		usedUnspents[i] = true;

		// add input to input parameters
		json_array_append(inputParams, input);
		json_decref(input);
		numInputs += 1;
	
		// add amount from utxo data to total amount
		totalAmnt += json_real_value(json_object_get(utxo, "amount"));

		// calculate size, fee, and change amount
		size = TX_BASE_SZ + (TX_INPUT_SZ*numInputs) + TX_OUTPUT_SZ + TX_DATA_BASE_SZ + txDataSz;
		fee = feePerByte * size;
		changeAmnt = totalAmnt - fee;
		// drop the change if less than cost of adding an additional input
		if (changeAmnt < feePerByte*(TX_INPUT_SZ)) { fee = feePerByte*(size-TX_OUTPUT_SZ); changeLost = changeAmnt; changeAmnt = 0; }

		if (totalAmnt >= fee && (changeAmnt > TX_DUST_AMNT || changeAmnt == 0)) { break; }
	}	
	for (int i=0; i<numUnspents; i++) { if (usedUnspents[i]) { json_array_remove(rp->unspents, i); } }

	// give up if insufficient funds
	if (totalAmnt < fee) { json_decref(inputParams); return CWS_FUNDS_NO; }

	// get change address and copy to memory
	char changeAddr[B_ADDRESS_BUF]; changeAddr[0] = 0;
	if ((status = rpcCall(rp, RPC_M_GETRAWCHANGEADDRESS, NULL, &jsonResult)) != CW_OK) {
		json_decref(inputParams);
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	strncat(changeAddr, json_string_value(jsonResult), B_ADDRESS_BUF-1);
	json_decref(jsonResult);
	jsonResult = NULL;
	if (strlen(changeAddr) == B_ADDRESS_BUF-1) {
		fprintf(stderr, "B_ADDRESS_BUF may not set high enough, probably needs to be updated for a new standard; problem with cashsendtools\n");
		json_decref(inputParams);
		return CW_SYS_ERR;
	}

	// construct output for data 
	if ((outputParams = json_object()) == NULL) {
		perror("json_object() failed");
		json_decref(inputParams);
		return CW_SYS_ERR;
	}	
	json_object_set_new(outputParams, "data", json_string(hexDatasC > 1 ? txHexData : *hexDatas));

	// construct output for change (if more than dust)	
	if (changeAmnt > TX_DUST_AMNT) {
		char changeAmntStr[B_AMNT_STR_BUF];
		if (snprintf(changeAmntStr, B_AMNT_STR_BUF, "%.8f", changeAmnt) > B_AMNT_STR_BUF-1) {
			fprintf(stderr, "B_AMNT_STR_BUF not set high enough; problem with cashsendtools\n");
			json_decref(inputParams);
			json_decref(outputParams);
			return CW_SYS_ERR;
		}
		json_object_set_new(outputParams, changeAddr, json_string(changeAmntStr));
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
			fprintf(stderr, "collective hex datas too big; sendTxAttempt() may need update in cashsendtools if the standard has changed\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		uint8_t txDataSzByte[1] = { (uint8_t)txDataSz };
		char txDataSzHex[2];
		byteArrToHexStr((const char *)txDataSzByte, 1, txDataSzHex);
		char *rtEditPtr; char *rtEditPtrS;
		if ((rtEditPtr = rtEditPtrS = strstr(rawTx, txHexData)) == NULL) {
			fprintf(stderr, "rawTx parsing error in sendTxAttempt(), attached hex data not found; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		bool opRetFound = false;
		for (; !(opRetFound = !strncmp(rtEditPtr, TX_OP_RETURN, 2)) && rtEditPtr-rawTx > 0; rtEditPtr -= 2);
		if (!opRetFound) {
			fprintf(stderr, "rawTx parsing error in sendTxAttempt(), op return code not found; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		if ((rtEditPtr -= 2)-rawTx <= 0) {
			fprintf(stderr, "rawTx parsing error in sendTxAttempt(), parsing rawTx arrived at invalid location; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		rtEditPtr[0] = txDataSzHex[0]; rtEditPtr[1] = txDataSzHex[1];

		int removed = rtEditPtrS - rtEditPtr;
		if (removed < 0) {
			fprintf(stderr, "rawTx parsing error in sendTxAttempt(), editing pointer locations wrong; problem with cashsendtools\n");
			free(rawTx);
			return CW_SYS_ERR;
		}
		strcpy(rtEditPtr, rtEditPtrS); rawTx[strlen(rawTx)-removed] = 0;
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
		fprintf(stderr, "error occurred in signing raw transaction; problem with cashsendtools\n\nraw tx:\n%s\n\n",
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
		rp->costAccrued += fee + changeLost;
		strncpy(resTxid, json_string_value(jsonResult), CW_TXID_CHARS);
		resTxid[CW_TXID_CHARS] = 0;
		fprintf(stderr, "-");
	} else if (status == CWS_RPC_NO) {
		const char *msg = json_string_value(json_object_get(jsonResult, "message"));
		if (strstr(msg, "too-long-mempool-chain")) { status = CWS_CONFIRMS_NO; }
		else if (strstr(msg, "insufficient priority")) { status = CWS_FEE_NO; }
		else if (strstr(msg, "txn-mempool-conflict") || strstr(msg, "Missing inputs")) { status = CWS_INPUTS_NO; }
		else { fprintf(stderr, "\nunhandled RPC error on sendrawtransaction\n"); }
	} else {
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	json_decref(jsonResult);

	return status;
}

static CW_STATUS sendTx(const char **hexDatas, int hexDatasC, struct CWS_rpc_pack *rp, char *resTxid) {
	bool printed = false;
	int count = 0; 
	double balance;
	double balanceN;
	CW_STATUS checkBalStatus;
	CW_STATUS status;
	do { 
		status = sendTxAttempt(hexDatas, hexDatasC, rp, true, true, resTxid);
		switch (status) {
			case CW_OK:
				break;	
			case CWS_FUNDS_NO:
				if ((checkBalStatus = checkBalance(rp, &balance)) != CW_OK) { return checkBalStatus; }
				do {
					if (!printed) { fprintf(stderr, "Insufficient balance, send more funds..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					fprintf(stderr, ".");
					if ((checkBalStatus = checkBalance(rp, &balanceN)) != CW_OK) { return checkBalStatus; }
				} while (balanceN <= balance);
				break;
			case CWS_CONFIRMS_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, rp, false, true, resTxid)) == CWS_CONFIRMS_NO) {
					if (!printed) { fprintf(stderr, "Waiting on confirmations..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					fprintf(stderr, ".");
				}
				break;
			case CWS_FEE_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, rp, true, false, resTxid)) == CWS_FEE_NO) {
					if (!printed) { fprintf(stderr, "Fee problem, attempting to resolve..."); printed = true; }
					fprintf(stderr, ".");
				}
				break;
			case CWS_INPUTS_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, rp, true, true, resTxid)) == CWS_INPUTS_NO) {
					if (!printed) { fprintf(stderr, "Bad UTXOS, attempting to resolve..."); printed = true; }
					if (rp->unspents && json_array_size(rp->unspents) == 0 && ++count >= 2) { break; }
				}
				if (status == CWS_INPUTS_NO) { return status; }
				break;
			case CWS_RPC_NO:
				fprintf(stderr, "RPC response error: %s\n", rp->errMsg);
				return status;
			default:
				return status;
		}
	} while (status != CW_OK);

	return status;
}

static CW_STATUS intToNetHexStr(void *uintPtr, int numBytes, char *hexStr) {
	unsigned char bytes[numBytes];
	
	uint16_t uint16 = 0; uint32_t uint32 = 0;
	bool isShort = false;
	switch (numBytes) {
		case sizeof(uint16_t):
			isShort = true;
			uint16 = htons(*(uint16_t *)uintPtr);
			break;
		case sizeof(uint32_t):
			uint32 = htonl(*(uint32_t *)uintPtr);
			break;
		default:
			fprintf(stderr, "invalid numBytes specified for nIntToHexStr, int must be 2 or 4 bytes; problem with cashsendtools\n");
			return CW_SYS_ERR;
	}

	for (int i=0; i<numBytes; i++) {
		bytes[i] = ((isShort ? uint16 : uint32) >> i*8) & 0xFF;
	}

	byteArrToHexStr((const char *)bytes, numBytes, hexStr);
	return CW_OK;
}

static void hexAppendMetadata(struct CW_file_metadata *md, char *hexStr) {	
	char chainLenHex[CW_MD_CHARS(length)+1]; char treeDepthHex[CW_MD_CHARS(depth)+1];
	char fTypeHex[CW_MD_CHARS(type)+1]; char pVerHex[CW_MD_CHARS(pVer)+1];

	intToNetHexStr(&md->length, CW_MD_BYTES(length), chainLenHex);
	intToNetHexStr(&md->depth, CW_MD_BYTES(depth), treeDepthHex);
	intToNetHexStr(&md->type, CW_MD_BYTES(type), fTypeHex);
	intToNetHexStr(&md->pVer, CW_MD_BYTES(pVer), pVerHex);

	strcat(hexStr, chainLenHex);
	strcat(hexStr, treeDepthHex);
	strcat(hexStr, fTypeHex);
	strcat(hexStr, pVerHex);
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
	if (!params->recoveryStream) { fprintf(stderr, "\nrecoveryStream is NULL pointer"); err = true; goto cleanup; }

	// write recovery info string to recovery stream	
	if (fputs(info.data, params->recoveryStream) == EOF) { perror("fputs() failed"); err = true; goto cleanup; }

	// write recovery data to recovery stream
	rewind(recoveryData);
	char buf[RECOVERY_DATA_BUF];
	size_t n;
	while ((n = fread(buf, 1, RECOVERY_DATA_BUF, recoveryData)) > 0) {
		if (fwrite(buf, 1, n, params->recoveryStream) < n) { perror("fwrite() failed"); err = true; goto cleanup; }	
	}
	if (ferror(recoveryData)) { perror("fread() failed"); err = true; goto cleanup; }

	cleanup:
		freeDynamicMemory(&info);
		if (err) { fprintf(stderr, "\nERROR: failed to write recovery data\n"); }
		return !err;
}

static CW_STATUS sendFileChain(FILE *fp, struct CWS_rpc_pack *rp, CW_TYPE cwType, int treeDepth, char *resTxid) { 
	char hexChunk[CW_TX_DATA_CHARS + 1]; const char *hexChunkPtr = hexChunk;
	char txid[CW_TXID_CHARS+1]; txid[0] = 0;
	char buf[treeDepth ? CW_TX_DATA_CHARS : CW_TX_DATA_BYTES];
	int metadataLen = treeDepth ? CW_METADATA_CHARS : CW_METADATA_BYTES;
	int readSz = treeDepth ? 2 : 1;
	long size = fileSize(fileno(fp));
	int toRead = size <= sizeof(buf) ? size : sizeof(buf);
	int read;
	bool begin = true; bool end = size+metadataLen <= sizeof(buf);
	CW_STATUS sendTxStatus;
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
		if (end) { hexAppendMetadata(&md, hexChunk); }
		if ((sendTxStatus = sendTx(&hexChunkPtr, 1, rp, txid)) != CW_OK) { return sendTxStatus; }
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

static CW_STATUS sendFileTreeLayer(FILE *fp, struct CWS_rpc_pack *rp, CW_TYPE cwType, int depth, int *numTxs, FILE *treeFp) {
	char hexChunk[CW_TX_DATA_CHARS + 1]; const char *hexChunkPtr = hexChunk;
	char txid[CW_TXID_CHARS+1]; txid[0] = 0;
	int metadataLen = depth ? CW_METADATA_CHARS : CW_METADATA_BYTES;
	int dataLen = depth ? CW_TX_DATA_CHARS : CW_TX_DATA_BYTES;
	char buf[dataLen];
	*numTxs = 0;
	CW_STATUS sendTxStatus;
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
				hexAppendMetadata(&md, hexChunk);
			} else { ++*numTxs; }
		}

		if ((sendTxStatus = sendTx(&hexChunkPtr, 1, rp, txid)) != CW_OK) { return sendTxStatus; }
		++*numTxs;	
		if (fputs(txid, treeFp) == EOF) { perror("fputs() failed"); return CW_SYS_ERR; }
	}
	if (ferror(fp)) { perror("file error on fread()"); return CW_SYS_ERR; }

	return CW_OK;
}

static CW_STATUS sendFileTree(FILE *fp, struct CWS_rpc_pack *rp, struct CWS_params *params, int depth, double *fundsLost, char *resTxid) {
	CW_STATUS status;
	double safeCost = rp->costAccrued;
	bool recover = false;

	FILE *tfp = NULL;

	if (params->maxTreeDepth >= 0 && depth >= params->maxTreeDepth) {
		if ((status = sendFileChain(fp, rp, params->cwType, depth, resTxid)) != CW_OK) { recover = true; goto cleanup; }
	}

	if ((tfp = tmpfile()) == NULL) { perror("tmpfile() failed"); return CW_SYS_ERR; }

	int numTxs;
	if ((status = sendFileTreeLayer(fp, rp, params->cwType, depth, &numTxs, tfp)) != CW_OK) { recover = true; goto cleanup; }
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

	status = sendFileTree(tfp, rp, params, depth+1, fundsLost, resTxid);

	cleanup:
		if (tfp) { fclose(tfp); }
		if (status != CW_OK && recover) {
			if (fundsLost) { *fundsLost = rp->costAccrued; }
			if (depth > 0) {
				fprintf(stderr, "\nCritical error; saving recovery data...\n");
				if (saveRecoveryData(fp, depth, params)) {
					fprintf(stderr, "Recovery data saved!\n");
					if (fundsLost) { *fundsLost -= safeCost; }
				} else { fprintf(stderr, "Failed to save recovery data; all data for this file lost\n"); }
			} else { fprintf(stderr, "\nCritical error; all data lost\n"); }		
		}
		return status;
}

static CW_STATUS sendFileFromStream(FILE *stream, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, int depth, double *fundsLost, char *resTxid) {
	if (params->cwType == CW_T_MIMESET) {
		fprintf(stderr, "WARNING: params specified type CW_T_MIMESET, but cashsendtools cannot determine mimetype when sending from stream;\n"
				 "defaulting to CW_T_FILE\n");
		params->cwType = CW_T_FILE;
	}
	CW_STATUS status = sendFileTree(stream, rpcPack, params, depth, fundsLost, resTxid);
	fprintf(stderr, "\n");
	return status;
}

static CW_STATUS sendFileFromPath(const char *path, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, double *fundsLost, char *resTxid) {
	FILE *fp;
	if ((fp = fopen(path, "rb")) == NULL) { perror("fopen() failed"); return CW_SYS_ERR; }	

	CW_STATUS status;	

	if (params->cwType == CW_T_MIMESET) {
		if ((status = CWS_set_cw_mime_type_by_extension(path, params)) != CW_OK) { goto cleanup; }
	}

	status = sendFileFromStream(fp, rpcPack, params, 0, fundsLost, resTxid);

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

static CW_STATUS initRpc(struct CWS_params *params, struct CWS_rpc_pack *rpcPack) {
	bitcoinrpc_global_init();
	rpcPack->cli = bitcoinrpc_cl_init_params(params->rpcUser, params->rpcPass, params->rpcServer, params->rpcPort);
	if (rpcPack->cli == NULL) { perror("bitcoinrpc_cl_init_params() failed"); return CWS_RPC_ERR;  }	

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
			default:
				fprintf(stderr, "initRpcMethods() reached unexpected identifier; rpc method #defines probably incorrect in cashsendtools\n");
				continue;
		}

		rpcPack->methods[i] = bitcoinrpc_method_init_params(method, mParams);
		if (mParams != NULL) { json_decref(mParams); }
		if (rpcPack->methods[i] == NULL) {
			fprintf(stderr, "bitcoinrpc_method_init() failed for rpc method %d\n", i);
			perror(NULL);
			return CW_SYS_ERR;
		}
		if (nonStdName != NULL) { bitcoinrpc_method_set_nonstandard(rpcPack->methods[i], nonStdName); }
	}
	rpcPack->unspents = NULL;
	rpcPack->costAccrued = 0;
	rpcPack->errMsg[0] = 0;

	return CW_OK;
}

static void cleanupRpc(struct CWS_rpc_pack *rpcPack) {
	if (rpcPack->cli) {
		bitcoinrpc_cl_free(rpcPack->cli);
		for (int i=0; i<RPC_METHODS_COUNT; i++) { if (rpcPack->methods[i]) { bitcoinrpc_method_free(rpcPack->methods[i]); } }
	}
	if (rpcPack->unspents) { json_decref(rpcPack->unspents); }
	
	bitcoinrpc_global_cleanup();
}
