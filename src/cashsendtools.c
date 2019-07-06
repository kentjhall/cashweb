#include "cashsendtools.h"

#define ERR_WAIT_CYCLE 5
#define EXTRA_PUSHDATA_BYTE_THRESHOLD 75

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
#define TX_DUST_AMNT 0.00000545
#define TX_BASE_SZ 10
#define TX_INPUT_SZ 148
#define TX_OUTPUT_SZ 34
#define TX_DATA_BASE_SZ 10

static bitcoinrpc_cl_t *rpcClient = NULL;
static struct bitcoinrpc_method *rpcMethods[RPC_METHODS_COUNT] = { NULL };

static long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

static void fileBytesToHexStr(char *hex, const char *fileBytes, int n) {
	for (int i=0; i<n; i++) {
		hex[i*2]   = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) >> 4];
		hex[i*2+1] = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) & 0x0F];
	}
	hex[n*2] = 0;
}

static int txDataSize(const char *hexData) {
	int dataSize = strlen(hexData)/2;
	return dataSize > EXTRA_PUSHDATA_BYTE_THRESHOLD ? dataSize+2 : dataSize+1; 
}

static void initRpc(bitcoinrpc_cl_t *rpcCli) {
	if (rpcClient != NULL) { return; }
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
				fprintf(stderr, "initRpcMethods() reached unexpected identifier; RPC_METHODS_COUNT probably incorrect in cashsendtools\n"); return;
		}

		if (rpcMethods[i] == NULL) { rpcMethods[i] = bitcoinrpc_method_init_params(method, params); }
		if (params != NULL) { json_decref(params); }
		if (nonStdName != NULL) { bitcoinrpc_method_set_nonstandard(rpcMethods[i], nonStdName); }
		if (!rpcMethods[i]) { fprintf(stderr, "bitcoinrpc_method_init() failed for rpc method %d\n", i); die(NULL); }
	}
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

static int sendTxAttempt(const char *hexData, bool useUnconfirmed, char **resultTxid) {
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

	int txDataSz = txDataSize(hexData);
	int size;
	int numInputs = 0;
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

		size = TX_BASE_SZ + (TX_INPUT_SZ*numInputs) + TX_OUTPUT_SZ + TX_DATA_BASE_SZ + txDataSz;
		fee = feePerByte * size;
		changeAmnt = totalAmnt - fee;

		if (totalAmnt >= fee && changeAmnt > TX_DUST_AMNT) { break; }
	}	
	json_decref(jsonResult);

	// give up if insufficient funds
	if (totalAmnt < fee) { json_decref(inputParams); return CWS_FUNDS_NO; }

	// get change address and copy to memory
	char changeAddr[B_ADDRESS_BUF];
	rpcCall(RPC_M_GETRAWCHANGEADDRESS, NULL, &jsonResult);
	if (strlcpy(changeAddr, json_string_value(jsonResult), B_ADDRESS_BUF) > B_ADDRESS_BUF-1) { die("B_ADDRESS_BUF not set high enough, probably needs to be updated for a new standard; problem with cashsendtools"); }
	json_decref(jsonResult);

	// construct outputs for data and change (if it is more than dust)
	if ((outputParams = json_object()) == NULL) { die("json_object() failed"); }	
	json_object_set_new(outputParams, "data", json_string(hexData));
	if (changeAmnt > TX_DUST_AMNT) {
		char changeAmntStr[B_AMNT_STR_BUF];
		if (snprintf(changeAmntStr, B_AMNT_STR_BUF, "%.8f", changeAmnt) > B_AMNT_STR_BUF-1) { die("B_AMNT_STR_BUF not set high enough; problem with cashsendtools"); }
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

	// construct params for signed tx from raw tx
	if ((params = json_array()) == NULL) { die("json_array() failed"); }
	json_array_append_new(params, json_string(rawTx));
	free(rawTx);

	// sign the raw transaction
	char *signedTx;
	rpcCall(RPC_M_SIGNRAWTRANSACTIONWITHWALLET, params, &jsonResult);
	json_decref(params);
	if ((signedTx = strdup(json_string_value(json_object_get(jsonResult, "hex")))) == NULL) { die("strdup() failed"); }
	json_decref(jsonResult);

	// construct params for sending the transaction from signed tx
	if ((params = json_array()) == NULL) { die("json_array() failed"); }
	json_array_append_new(params, json_string(signedTx));
	free(signedTx);

	// send transaction and handle potential errors
	int status = rpcCall(RPC_M_SENDRAWTRANSACTION, params, &jsonResult);
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

static void sendTx(const char *hexData, char **txid) {
	bool printed = false;
	double balance;
	int status;
	do { 
		status = sendTxAttempt(hexData, true, txid);
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
				while ((status = sendTxAttempt(hexData, false, txid)) == CWS_CONFIRMS_NO) {
					if (!printed) { fprintf(stderr, "Waiting on confirmations..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					fprintf(stderr, ".");
				}
				break;
			case CWS_FEE_NO:
				while ((status = sendTxAttempt(hexData, true, txid)) == CWS_FEE_NO) {
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

static char *fileHandleByTxTree(FILE *fp, FILE *tempfp, int depth) {
	char hexChunk[TX_DATA_CHARS + 1];
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	char buf[depth ? TX_DATA_CHARS : TX_DATA_BYTES];
	int n;
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
		if (!depth) {
			fileBytesToHexStr(hexChunk, buf, n);
		} else if (n%2 == 0) { 
			memcpy(hexChunk, buf, n); 
			hexChunk[n] = 0; 
		} else { die("fread() error"); }
		sendTx(hexChunk, &txid);
		if (fputs(txid, tempfp) == EOF) { die("fputs() failed"); }
	}
	if (fputs(TREE_SUFFIX, tempfp) == EOF) { die("fputs() failed"); }
	if (ferror(fp)) { die("file error on fread()"); }
	return txid;
}

static char *fileHandleByTxChain(FILE *fp, long size, int isHex) { 
	char hexChunk[TX_DATA_CHARS + 1];
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	txid[0] = 0;
	char buf[isHex ? TX_DATA_CHARS : TX_DATA_BYTES];
	int toRead = size < sizeof(buf) ? size : sizeof(buf);
	int read;
	int begin = 1; int end = 0;
	int loc = 0;
	if (fseek(fp, -toRead, SEEK_END) != 0) { die("fseek() failed"); }
	while ((read = fread(buf, 1, toRead, fp)) > 0) {
		if (!isHex) {
			fileBytesToHexStr(hexChunk, buf, read);
		} else {
			memcpy(hexChunk, buf, toRead);
			hexChunk[toRead] = 0;
		}
		strcat(hexChunk, txid);
		sendTx(hexChunk, &txid);
		if (end) { break; }
		if (begin) { toRead -= isHex ? TXID_CHARS : TXID_BYTES; begin = 0; }
		if ((loc = ftell(fp))-read < toRead) {
			toRead = loc-read;
			if (fseek(fp, 0, SEEK_SET) != 0) { die("fseek() failed"); }
			end = 1; 
		} else if (fseek(fp, -read-toRead, SEEK_CUR) != 0) { die("fseek() failed"); }
	}
	if (ferror(fp)) { die("file error on fread()"); }
	fprintf(stderr, "\n");
	return txid;
}

static inline char *sendFileChain(FILE *fp, int isHex) {
	return fileHandleByTxChain(fp, fileSize(fileno(fp)), isHex);
}

static char *sendFileTree(FILE *fp, int depth, int maxDepth) {
	if (maxDepth >= 0 && depth >= maxDepth) { return sendFileChain(fp, depth); }

	FILE *tfp;
	if ((tfp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	char *txidF = fileHandleByTxTree(fp, tfp, depth);
	if ((int)ceil((double)fileSize(fileno(fp))/(depth ? TX_DATA_CHARS : TX_DATA_BYTES)) < 2) { fclose(tfp); return txidF; }
	free(txidF);

	rewind(tfp);
	char *txid = sendFileTree(tfp, depth+1, maxDepth);
	fclose(tfp);
	fprintf(stderr, "\n");
	return txid;
}

static inline char *sendFp(FILE *fp, int maxTreeDepth) {
	return sendFileTree(fp, 0, maxTreeDepth);
}

char *sendFile(const char *filePath, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff) {
	initRpc(rpcCli);

	double bal;
	if (balanceDiff) { bal = checkBalance(); }

	FILE *fp;
	if ((fp = fopen(filePath, "rb")) == NULL) { die("fopen() failed"); }	
	char *txid = sendFp(fp, maxTreeDepth);

	if (balanceDiff) { *balanceDiff = bal - checkBalance(); }

	fclose(fp);
	freeRpcMethods();
	return txid;
}

char *sendDir(const char *dirPath, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff) {
	initRpc(rpcCli);

	FTS *ftsp;
	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	char const *paths[] = { dirPath, NULL };
	if ((ftsp = fts_open((char * const *)paths, fts_options, NULL)) == NULL) { die("fts_open() failed"); }
	int dirPathLen = strlen(dirPath);

	FILE *tmpDirFp;
	if ((tmpDirFp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	double bal;
	if (balanceDiff) { bal = checkBalance(); }

	char *txid;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL) {
		if (p->fts_info == FTS_F && strncmp(p->fts_path, dirPath, dirPathLen) == 0) {
			fprintf(stderr, "Sending %s", p->fts_path+dirPathLen);
			txid = sendFile(p->fts_path, maxTreeDepth, NULL, NULL);
			fprintf(tmpDirFp, "%s\n%s\n", p->fts_path+dirPathLen, txid);
			free(txid);
		}
	}
	fprintf(stderr, "Sending root directory file");
	rewind(tmpDirFp);
	txid = sendFp(tmpDirFp, maxTreeDepth);

	if (balanceDiff) { *balanceDiff = bal - checkBalance(); }

	fclose(tmpDirFp);
	fts_close(ftsp);
	freeRpcMethods();
	return txid;	
}
