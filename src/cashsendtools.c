#include "cashsendtools.h"

#define ERR_WAIT_CYCLE 5

// rpc method #defines
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

// send data #defines
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

static bitcoinrpc_cl_t *rpcClient = NULL;
static struct bitcoinrpc_method *rpcMethods[RPC_METHODS_COUNT] = { NULL };

/*
 * puts int to network byte order (big-endian) and converts to hex string, written to passed memory location
 * supports 2 and 4 byte integers
 */
static void intToNetHexStr(void *uintPtr, int numBytes, char *hex) {
	unsigned char bytes[numBytes];
	
	uint16_t uint16; uint32_t uint32;
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
			fprintf(stderr, "invalid numBytes specified for nIntToHexStr, int must be 2, 4, or 8 bytes; problem with cashsendtools\n");
			exit(1);
	}

	for (int i=0; i<numBytes; i++) {
		bytes[i] = ((isShort ? uint16 : uint32) >> i*8) & 0xFF;
	}

	byteArrToHexStr((const char *)bytes, numBytes, hex);
}

static bool initRpc(bitcoinrpc_cl_t *rpcCli) {
	if (rpcClient != NULL) { return false; }
	rpcClient = rpcCli;

	BITCOINRPC_METHOD method;
	json_t *params;
	char *nonStdName;
	for (int i=0; i<RPC_METHODS_COUNT; i++) {
		params = NULL;
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
				if ((params = json_array()) == NULL) { die("json_array() failed"); }
				json_array_append_new(params, json_integer(0));
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

		if (rpcMethods[i] == NULL) { rpcMethods[i] = bitcoinrpc_method_init_params(method, params); }
		if (params != NULL) { json_decref(params); }
		if (nonStdName != NULL) { bitcoinrpc_method_set_nonstandard(rpcMethods[i], nonStdName); }
		if (!rpcMethods[i]) { fprintf(stderr, "bitcoinrpc_method_init() failed for rpc method %d\n", i); die(NULL); }
	}
	return true;
}

static void freeRpcMethods() {
	for (int i=0; i<RPC_METHODS_COUNT; i++) { if (rpcMethods[i]) { bitcoinrpc_method_free(rpcMethods[i]); } }
}

static int rpcCallAttempt(int rpcMethodI, json_t *params, json_t **jsonResult) {
	bitcoinrpc_resp_t *bResponse = bitcoinrpc_resp_init();
	if (!bResponse) { die("bitcoinrpc_resp_init() failed"); }
	bitcoinrpc_err_t bError;

	struct bitcoinrpc_method *rpcMethod = rpcMethods[rpcMethodI];
	if (!rpcMethod) { fprintf(stderr, "rpc method (identifier %d) not initialized; probably an issue with cashsendtools\n", rpcMethodI); die(NULL); }
	if (params != NULL && bitcoinrpc_method_set_params(rpcMethod, params) != BITCOINRPCE_OK) { fprintf(stderr, "count not set params for rpc method (identifier %d); probably an issue with cashsendtools\n", rpcMethodI); die(NULL); }
	bitcoinrpc_call(rpcClient, rpcMethod, bResponse, &bError);	

	int status = CW_OK;
	if (bError.code != BITCOINRPCE_OK) { fprintf(stderr, "rpc error %d [%s]\n", bError.code, bError.msg); status = CWS_RPC_ERR; goto cleanup; }

	json_t *jsonResponse = bitcoinrpc_resp_get(bResponse);
	*jsonResult = json_object_get(jsonResponse, "result");
	if (json_is_null(*jsonResult)) {
		json_decref(*jsonResult);
		*jsonResult = json_object_get(jsonResponse, "error");
		status = CWS_RPC_NO;
	} 
	json_incref(*jsonResult);
	json_decref(jsonResponse);

	cleanup:
		bitcoinrpc_resp_free(bResponse);
		return status;
}

static int rpcCall(int rpcMethodI, json_t *params, json_t **jsonResult) {
	bool printed = false;
	int status;
	while ((status = rpcCallAttempt(rpcMethodI, params, jsonResult)) == CWS_RPC_ERR) {
		if (!printed) {
			fprintf(stderr, "\nRPC request failed, please ensure bitcoind is running and configured correctly; retrying...");
			printed = true;
		}
		sleep(ERR_WAIT_CYCLE);
		fprintf(stderr, ".");
	}
	if (printed) { fprintf(stderr, "\n"); }
	return status;
}

static double checkBalance() {
	json_t *jsonResult;

	rpcCall(RPC_M_GETBALANCE, NULL, &jsonResult);
	double balance = json_real_value(jsonResult);
	json_decref(jsonResult);

	rpcCall(RPC_M_GETUNCONFIRMEDBALANCE, NULL, &jsonResult);
	balance += json_real_value(jsonResult);
	json_decref(jsonResult);
	
	return balance;
}

static int txDataSize(int hexDataLen, bool *extraByte) {
	int dataSize = hexDataLen/2;
	int added = dataSize > TX_OP_PUSHDATA1_THRESHOLD ? 2 : 1;
	if (extraByte != NULL) { *extraByte = added > 1; }
	return dataSize + added; 
}

static int sendTxAttempt(const char **hexDatas, int hexDatasC, bool useUnconfirmed, char **resultTxid) {
	json_t *jsonResult;
	json_t *params;
	
	// get estimated fee per byte and copy to memory
	double feePerByte;
	rpcCall(RPC_M_ESTIMATEFEE, NULL, &jsonResult);
	feePerByte = json_real_value(jsonResult)/1000;
	json_decref(jsonResult);

	// get unspent utxos (may or may not include unconfirmed) and resolve the count
	int numUnspents;
	rpcCall(useUnconfirmed ? RPC_M_LISTUNSPENT_0 : RPC_M_LISTUNSPENT, NULL, &jsonResult);
	if ((numUnspents = json_array_size(jsonResult)) == 0) { return useUnconfirmed ? CWS_FUNDS_NO : CWS_CONFIRMS_NO; }

	json_t *utxo;
	json_t *input;
	json_t *inputParams;
	json_t *outputParams;

	// calculate tx data size and prepare datas if more than one pushdata
	int txDataSz = 0;
	char txHexData[TX_RAW_DATA_CHARS+1]; txHexData[0] = 0;
	int hexLen; int dataSz; bool extraByte; uint8_t szByte[1]; char szHex[2];
	for (int i=0; i<hexDatasC; i++) {
		txDataSz += txDataSize((hexLen = strlen(hexDatas[i])), &extraByte);
		if (hexDatasC > 1) {
			if ((dataSz = hexLen/2) > 255) { fprintf(stderr, "sendTxAttempt() doesn't support data >255 bytes; cashsendtools may need revision if this standard has changed\n"); exit(1); }
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

	char txid[TXID_CHARS+1];
	int vout;
	if ((inputParams = json_array()) == NULL) { die("json_array() failed"); }
	
	// iterate through unspent utxos
	for (int i=numUnspents-1; i>=0; i--) {
		// pull utxo data
		utxo = json_array_get(jsonResult, i);

		// copy txid from utxo data to memory
		txid[0] = 0;
		strncat(txid, json_string_value(json_object_get(utxo, "txid")), TXID_CHARS);

		// copy vout from utxo data to memory
		vout = json_integer_value(json_object_get(utxo, "vout"));

		// construct input from txid and vout
		if ((input = json_object()) == NULL) { die("json_object() failed"); }
		json_object_set_new(input, "txid", json_string(txid));
		json_object_set_new(input, "vout", json_integer(vout));

		// add input to input parameters
		json_array_append(inputParams, input);
		numInputs += 1;
	
		// add amount from utxo data to total amount
		totalAmnt += json_real_value(json_object_get(utxo, "amount"));

		// calculate size, fee, and change amount
		size = TX_BASE_SZ + (TX_INPUT_SZ*numInputs) + TX_OUTPUT_SZ + TX_DATA_BASE_SZ + txDataSz;
		fee = feePerByte * size;
		changeAmnt = totalAmnt - fee;

		if (totalAmnt >= fee && changeAmnt > TX_DUST_AMNT) { break; }
	}	
	json_decref(jsonResult);

	// give up if insufficient funds
	if (totalAmnt < fee) { json_decref(inputParams); return CWS_FUNDS_NO; }

	// get change address and copy to memory
	char changeAddr[B_ADDRESS_BUF]; changeAddr[0] = 0;
	rpcCall(RPC_M_GETRAWCHANGEADDRESS, NULL, &jsonResult);
	strncat(changeAddr, json_string_value(jsonResult), B_ADDRESS_BUF-1);
	if (strlen(changeAddr) == B_ADDRESS_BUF-1) { fprintf(stderr, "B_ADDRESS_BUF may not set high enough, probably needs to be updated for a new standard; problem with cashsendtools"); exit(1); }
	json_decref(jsonResult);

	// construct output for data 
	if ((outputParams = json_object()) == NULL) { die("json_object() failed"); }	
	json_object_set_new(outputParams, "data", json_string(hexDatasC > 1 ? txHexData : *hexDatas));

	// construct output for change (if more than dust)	
	if (changeAmnt > TX_DUST_AMNT) {
		char changeAmntStr[B_AMNT_STR_BUF];
		if (snprintf(changeAmntStr, B_AMNT_STR_BUF, "%.8f", changeAmnt) > B_AMNT_STR_BUF-1) { fprintf(stderr, "B_AMNT_STR_BUF not set high enough; problem with cashsendtools"); exit(1); }
		json_object_set_new(outputParams, changeAddr, json_string(changeAmntStr));
	}

	// construct params from inputs and outputs
	if ((params = json_array()) == NULL) { die("json_array() failed"); }
	json_array_append(params, inputParams);
	json_decref(inputParams);
	json_array_append(params, outputParams);
	json_decref(outputParams);

	// create raw transaction from params
	char *rawTx;
	rpcCall(RPC_M_CREATERAWTRANSACTION, params, &jsonResult);
	json_decref(params);
	if ((rawTx = strdup(json_string_value(jsonResult))) == NULL) { die("strdup() failed"); }
	json_decref(jsonResult);

	// edit raw transaction for extra hex datas if present
	if (hexDatasC > 1) {
		if (++txDataSz > 255) { fprintf(stderr, "collective hex datas too big; sendTxAttempt() may need update in cashsendtools if the standard has changed\n"); exit(1); }
		uint8_t txDataSzByte[1] = { (uint8_t)txDataSz };
		char txDataSzHex[2];
		byteArrToHexStr((const char *)txDataSzByte, 1, txDataSzHex);
		char *rtEditPtr; char *rtEditPtrS;
		if ((rtEditPtr = rtEditPtrS = strstr(rawTx, txHexData)) == NULL) { fprintf(stderr, "rawTx parsing error in sendTxAttempt(), attached hex data not found; problem with cashsendtools\n"); exit(1); }
		bool opRetFound = false;
		for (; !(opRetFound = !strncmp(rtEditPtr, TX_OP_RETURN, 2)) && rtEditPtr-rawTx > 0; rtEditPtr -= 2);
		if (!opRetFound) { fprintf(stderr, "rawTx parsing error in sendTxAttempt(), op return code not found; problem with cashsendtools\n"); exit(1); }
		if ((rtEditPtr -= 2)-rawTx <= 0) { fprintf(stderr, "rawTx parsing error in sendTxAttempt(), parsing rawTx arrived at invalid location; problem with cashsendtools\n"); exit(1); }
		rtEditPtr[0] = txDataSzHex[0]; rtEditPtr[1] = txDataSzHex[1];

		int removed = rtEditPtrS - rtEditPtr;
		if (removed < 0) { fprintf(stderr, "rawTx parsing error in sendTxAttempt(), editing pointer locations wrong; problem with cashsendtools\n"); exit(1); }
		strcpy(rtEditPtr, rtEditPtrS); rawTx[strlen(rawTx)-removed] = 0;
	}

	// construct params for signed tx from raw tx
	if ((params = json_array()) == NULL) { die("json_array() failed"); }
	json_array_append_new(params, json_string(rawTx));

	// sign the raw transaction
	char *signedTx;
	int status = rpcCall(RPC_M_SIGNRAWTRANSACTIONWITHWALLET, params, &jsonResult);
	json_decref(params);
	if (status == CWS_RPC_NO) {
		fprintf(stderr, "error occurred in signing raw transaction; problem with cashsendtools\n\nraw tx:\n%s\n\nerror:\n%s",
			rawTx, json_dumps(jsonResult, 0));
		die(NULL);
	} else if (status != CW_OK) { fprintf(stderr, "unhandled error (identifier %d) returned on rpcCall(); problem with cashsendtools\n", status); die(NULL); }
	free(rawTx);
	if ((signedTx = strdup(json_string_value(json_object_get(jsonResult, "hex")))) == NULL) { die("strdup() failed"); }
	json_decref(jsonResult);

	// construct params for sending the transaction from signed tx
	if ((params = json_array()) == NULL) { die("json_array() failed"); }
	json_array_append_new(params, json_string(signedTx));
	free(signedTx);

	// send transaction and handle potential errors
	status = rpcCall(RPC_M_SENDRAWTRANSACTION, params, &jsonResult);
	json_decref(params);
	if (status == CWS_RPC_NO) {
		const char *msg = json_string_value(json_object_get(jsonResult, "message"));
		if (strstr(msg, "too-long-mempool-chain")) { status = CWS_CONFIRMS_NO; }
		else if (strstr(msg, "insufficient priority")) { status = CWS_FEE_NO; }
		else { fprintf(stderr, "unhandled sendrawtransaction error (%s); problem with cashsendtools\n", msg); die(NULL); }
		fprintf(stderr, "\n");
	} else if (status == CW_OK) {
		(*resultTxid)[0] = 0; strncat(*resultTxid, json_string_value(jsonResult), TXID_CHARS);
		fprintf(stderr, "-");
	} else { fprintf(stderr, "unhandled error (identifier %d) returned on rpcCall(); problem with cashsendtools\n", status); die(NULL); }
	json_decref(jsonResult);

	return status;
}

static void sendTx(const char **hexDatas, int hexDatasC, char **resultTxid) {
	bool printed = false;
	double balance;
	int status;
	do { 
		status = sendTxAttempt(hexDatas, hexDatasC, true, resultTxid);
		switch (status) {
			case CW_OK:
				break;
			case CWS_FUNDS_NO:
				balance = checkBalance();	
				while (checkBalance() <= balance) {
					if (!printed) { fprintf(stderr, "Insufficient balance, send more funds..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					fprintf(stderr, ".");
				}
				break;
			case CWS_CONFIRMS_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, false, resultTxid)) == CWS_CONFIRMS_NO) {
					if (!printed) { fprintf(stderr, "Waiting on confirmations..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					fprintf(stderr, ".");
				}
				break;
			case CWS_FEE_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, true, resultTxid)) == CWS_FEE_NO) {
					if (!printed) { fprintf(stderr, "Fee problem, attempting to resolve..."); printed = true; }
					fprintf(stderr, ".");
				}
				break;
			default:
				fprintf(stderr, "unexpected error on sendTxAttempt() (identifier %d); problem with cashsendtools\n", status); die(NULL);
		}
		if (printed) { fprintf(stderr, "\n"); }
	} while (status != CW_OK);
}

static void hexAppendMetadata(char *hexData, struct cwFileMetadata *md) {	
	char chainLenHex[CW_MD_CHARS(length)+1]; char treeDepthHex[CW_MD_CHARS(depth)+1];
	char fTypeHex[CW_MD_CHARS(type)+1]; char pVerHex[CW_MD_CHARS(pVer)+1];

	intToNetHexStr(&md->length, CW_MD_BYTES(length), chainLenHex);
	intToNetHexStr(&md->depth, CW_MD_BYTES(depth), treeDepthHex);
	intToNetHexStr(&md->type, CW_MD_BYTES(type), fTypeHex);
	intToNetHexStr(&md->pVer, CW_MD_BYTES(pVer), pVerHex);

	strcat(hexData, chainLenHex);
	strcat(hexData, treeDepthHex);
	strcat(hexData, fTypeHex);
	strcat(hexData, pVerHex);
}

static char *fileSendAsTxTree(FILE *fp, int cwFType, FILE *tempfp, int depth, int *numTxs) {
	char hexChunk[TX_DATA_CHARS + 1]; char *hexChunkPtr = hexChunk;
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	int metadataLen = depth ? CW_METADATA_CHARS : CW_METADATA_BYTES;
	int dataLen = depth ? TX_DATA_CHARS : TX_DATA_BYTES;
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
				struct cwFileMetadata md;
				initCwFileMetadata(&md, cwFType);
				md.depth = depth;
				hexAppendMetadata(hexChunk, &md);
			} else { ++*numTxs; }
		}

		sendTx((const char **)&hexChunkPtr, 1, &txid);
		++*numTxs;	
		if (fputs(txid, tempfp) == EOF) { die("fputs() failed"); }
	}
	if (ferror(fp)) { die("file error on fread()"); }
	return txid;
}

static char *fileSendAsTxChain(FILE *fp, int cwFType, long size, int treeDepth) { 
	char hexChunk[TX_DATA_CHARS + 1]; char *hexChunkPtr = hexChunk;
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	txid[0] = 0;
	char buf[treeDepth ? TX_DATA_CHARS : TX_DATA_BYTES];
	int metadataLen = treeDepth ? CW_METADATA_CHARS : CW_METADATA_BYTES;
	int readSz = treeDepth ? 2 : 1;
	int toRead = size <= sizeof(buf) ? size : sizeof(buf);
	int read;
	bool begin = true; bool end = size+metadataLen <= sizeof(buf);
	struct cwFileMetadata md;
	initCwFileMetadata(&md, cwFType);
	md.depth = treeDepth;
	int loc = 0;
	if (fseek(fp, -toRead, SEEK_END) != 0) { die("fseek() SEEK_END failed"); }
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
		if (end) { hexAppendMetadata(hexChunk, &md); }
		sendTx((const char **)&hexChunkPtr, 1, &txid);
		if (end) { break; }
		++md.length;
		if (begin) { toRead -= treeDepth ? TXID_CHARS : TXID_BYTES; begin = false; }
		if ((loc = ftell(fp))-read < toRead) {
			if (loc-read < toRead-metadataLen) { end = true; }
			toRead = loc-read;
			if (fseek(fp, 0, SEEK_SET) != 0) { die("fseek() SEEK_SET failed"); }
		} else if (fseek(fp, -read-toRead, SEEK_CUR) != 0) { die("fseek() SEEK_CUR failed"); }
	}
	if (ferror(fp)) { die("file error on fread()"); }
	return txid;
}

static inline char *sendFileChain(FILE *fp, int cwFType, int treeDepth) {
	return fileSendAsTxChain(fp, cwFType, fileSize(fileno(fp)), treeDepth);
}

static char *sendFileTree(FILE *fp, int cwFType, int depth, int maxDepth) {
	if (maxDepth >= 0 && depth >= maxDepth) { return sendFileChain(fp, cwFType, depth); }

	FILE *tfp;
	if ((tfp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	int numTxs;
	char *txidF = fileSendAsTxTree(fp, cwFType, tfp, depth, &numTxs);
	if (numTxs < 2) { fclose(tfp); return txidF; }
	free(txidF);

	rewind(tfp);
	char *txid = sendFileTree(tfp, cwFType, depth+1, maxDepth);
	fclose(tfp);
	return txid;
}

static inline char *sendFp(FILE *fp, int cwFType, int maxTreeDepth) {
	char *txid = sendFileTree(fp, cwFType, 0, maxTreeDepth);
	fprintf(stderr, "\n");
	return txid;
}

char *sendFile(const char *filePath, int cwType, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff) {
	bool rpcInitHere = initRpc(rpcCli);

	double bal;
	if (balanceDiff) { bal = checkBalance(); }

	FILE *fp;
	if ((fp = fopen(filePath, "rb")) == NULL) { die("fopen() failed"); }	
	char *txid = sendFp(fp, cwType, maxTreeDepth);

	if (balanceDiff) { *balanceDiff = bal - checkBalance(); }

	fclose(fp);
	if (rpcInitHere) { freeRpcMethods(); }
	return txid;
}

static int scanDirFileCount(char const *path[]) {
	FTS *ftsp;
	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	if ((ftsp = fts_open((char * const *)path, fts_options, NULL)) == NULL) { die("fts_open() failed"); }

	int count = 0;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL) { if (p->fts_info == FTS_F) { ++count; } }

	fts_close(ftsp);
	return count;
}

char *sendDir(const char *dirPath, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff) {
	bool rpcInitHere = initRpc(rpcCli);

	char const *path[] = { dirPath, NULL };
	int numFiles = scanDirFileCount(path);

	FTS *ftsp;
	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	if ((ftsp = fts_open((char * const *)path, fts_options, NULL)) == NULL) { die("fts_open() failed"); }
	int dirPathLen = strlen(dirPath);

	FILE *tmpDirFp;
	if ((tmpDirFp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	double bal;
	if (balanceDiff) { bal = checkBalance(); }

	char *txid;
	char txidsByteData[numFiles*TXID_BYTES];

	int count = 0;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL && count < numFiles) {
		if (p->fts_info == FTS_F && strncmp(p->fts_path, dirPath, dirPathLen) == 0) {
			fprintf(stderr, "Sending %s", p->fts_path+dirPathLen);

			txid = sendFile(p->fts_path, CW_T_FILE, maxTreeDepth, NULL, NULL);
			if (hexStrToByteArr(txid, 0, txidsByteData+(count*TXID_BYTES)) != TXID_BYTES) {
				fprintf(stderr, "invalid txid in fileSendAsTxTree\n"); die(NULL);
			}
			++count;
			free(txid);

			fprintf(tmpDirFp, "%s\n", p->fts_path+dirPathLen);
		}
	}
	fprintf(tmpDirFp, "\n");
	if (fwrite(txidsByteData, TXID_BYTES, numFiles, tmpDirFp) <= 0) { die("fwrite() to tmpDirFp failed"); }
	fprintf(stderr, "Sending root directory file");
	rewind(tmpDirFp);
	txid = sendFp(tmpDirFp, CW_T_DIR, maxTreeDepth);

	if (balanceDiff) { *balanceDiff = bal - checkBalance(); }

	fclose(tmpDirFp);
	fts_close(ftsp);
	if (rpcInitHere) { freeRpcMethods(); }
	return txid;	
}
