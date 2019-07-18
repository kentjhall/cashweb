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

struct CWS_rpc_pack {
	bitcoinrpc_cl_t *cli;
	struct bitcoinrpc_method *methods[RPC_METHODS_COUNT];
};

/*
 * puts int to network byte order (big-endian) and converts to hex string, written to passed memory location
 * supports 2 and 4 byte integers
 */
static CW_STATUS intToNetHexStr(void *uintPtr, int numBytes, char *hex) {
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

	byteArrToHexStr((const char *)bytes, numBytes, hex);
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

	return CW_OK;
}

static void cleanupRpc(struct CWS_rpc_pack *rpcPack) {
	if (rpcPack->cli) {
		bitcoinrpc_cl_free(rpcPack->cli);
		for (int i=0; i<RPC_METHODS_COUNT; i++) { if (rpcPack->methods[i]) { bitcoinrpc_method_free(rpcPack->methods[i]); } }
	}
	
	bitcoinrpc_global_cleanup();
}

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
			fprintf(stderr, "\nRPC request failed, please ensure bitcoind is running and configured correctly; retrying...");
			printed = true;
		}
		sleep(ERR_WAIT_CYCLE);
		fprintf(stderr, ".");
	}
	if (printed) { fprintf(stderr, "\n"); }
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

static CW_STATUS sendTxAttempt(const char **hexDatas, int hexDatasC, struct CWS_rpc_pack *rp, bool useUnconfirmed, char *resTxid) {
	CW_STATUS status;
	json_t *jsonResult = NULL;
	json_t *params;
	
	// get estimated fee per byte and copy to memory
	double feePerByte;
	if ((status = rpcCall(rp, RPC_M_ESTIMATEFEE, NULL, &jsonResult)) != CW_OK) {
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	feePerByte = json_real_value(jsonResult)/1000;
	json_decref(jsonResult);
	jsonResult = NULL;

	// get unspent utxos (may or may not include unconfirmed) and resolve the count
	int numUnspents;
	if ((status = rpcCall(rp, useUnconfirmed ? RPC_M_LISTUNSPENT_0 : RPC_M_LISTUNSPENT, NULL, &jsonResult)) != CW_OK) {
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
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
			if ((dataSz = hexLen/2) > 255) {
				fprintf(stderr, "sendTxAttempt() doesn't support data >255 bytes; cashsendtools may need revision if this standard has changed\n");
				json_decref(jsonResult);
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

	char txid[TXID_CHARS+1];
	int vout;
	if ((inputParams = json_array()) == NULL) { perror("json_array() failed"); json_decref(jsonResult); return CW_SYS_ERR; }
	
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
		if ((input = json_object()) == NULL) {
			perror("json_object() failed");
			json_decref(inputParams);
			json_decref(jsonResult);
			return CW_SYS_ERR;
		}
		json_object_set_new(input, "txid", json_string(txid));
		json_object_set_new(input, "vout", json_integer(vout));

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
		if (changeAmnt < feePerByte*(TX_INPUT_SZ)) { fee = feePerByte*(size-TX_OUTPUT_SZ); changeAmnt = 0; }

		if (totalAmnt >= fee && (changeAmnt > TX_DUST_AMNT || changeAmnt == 0)) { break; }
	}	
	json_decref(jsonResult);
	jsonResult = NULL;

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
	}

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
		fprintf(stderr, "error occurred in signing raw transaction; problem with cashsendtools\n\nraw tx:\n%s\n\nerror:\n%s",
			rawTx, json_dumps(jsonResult, 0));
		free(rawTx);
		json_decref(jsonResult);
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
	if (status == CWS_RPC_NO) {
		const char *msg = json_string_value(json_object_get(jsonResult, "message"));
		if (strstr(msg, "too-long-mempool-chain")) { status = CWS_CONFIRMS_NO; }
		else if (strstr(msg, "insufficient priority")) { status = CWS_FEE_NO; }
		else { fprintf(stderr, "unhandled sendrawtransaction error (%s); problem with cashsendtools\n", msg); }
		fprintf(stderr, "\n");
	} else if (status == CW_OK) {
		strncpy(resTxid, json_string_value(jsonResult), TXID_CHARS);
		resTxid[TXID_CHARS] = 0;
		fprintf(stderr, "-");
	} else {
		fprintf(stderr, "unhandled error (identifier %d) returned on rpcCall(); problem with cashsendtools\n", status);
		if (jsonResult) { json_decref(jsonResult); }
		return status;
	}
	json_decref(jsonResult);

	return status;
}

static CW_STATUS sendTx(const char **hexDatas, int hexDatasC, struct CWS_rpc_pack *rp, char *resTxid) {
	bool printed = false;
	double balance;
	double balanceN;
	CW_STATUS checkBalStatus;
	CW_STATUS status;
	do { 
		status = sendTxAttempt(hexDatas, hexDatasC, rp, true, resTxid);
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
				while ((status = sendTxAttempt(hexDatas, hexDatasC, rp, false, resTxid)) == CWS_CONFIRMS_NO) {
					if (!printed) { fprintf(stderr, "Waiting on confirmations..."); printed = true; }
					sleep(ERR_WAIT_CYCLE);
					fprintf(stderr, ".");
				}
				break;
			case CWS_FEE_NO:
				while ((status = sendTxAttempt(hexDatas, hexDatasC, rp, true, resTxid)) == CWS_FEE_NO) {
					if (!printed) { fprintf(stderr, "Fee problem, attempting to resolve..."); printed = true; }
					fprintf(stderr, ".");
				}
				break;
			default:
				return status;
		}
		if (printed) { fprintf(stderr, "\n"); }
	} while (status != CW_OK);

	return status;
}

static void hexAppendMetadata(char *hexData, struct CW_file_metadata *md) {	
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

static CW_STATUS sendFileChain(FILE *fp, struct CWS_rpc_pack *rp, int cwFType, int treeDepth, char *resTxid) { 
	char hexChunk[TX_DATA_CHARS + 1]; char *hexChunkPtr = hexChunk;
	char txid[TXID_CHARS+1];
	char buf[treeDepth ? TX_DATA_CHARS : TX_DATA_BYTES];
	int metadataLen = treeDepth ? CW_METADATA_CHARS : CW_METADATA_BYTES;
	int readSz = treeDepth ? 2 : 1;
	long size = fileSize(fileno(fp));
	int toRead = size <= sizeof(buf) ? size : sizeof(buf);
	int read;
	bool begin = true; bool end = size+metadataLen <= sizeof(buf);
	CW_STATUS sendTxStatus;
	struct CW_file_metadata md;
	init_CW_file_metadata(&md, cwFType);
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
		if (end) { hexAppendMetadata(hexChunk, &md); }
		if ((sendTxStatus = sendTx((const char **)&hexChunkPtr, 1, rp, txid)) != CW_OK) { return sendTxStatus; }
		if (end) { break; }
		++md.length;
		if (begin) { toRead -= treeDepth ? TXID_CHARS : TXID_BYTES; begin = false; }
		if ((loc = ftell(fp))-read < toRead) {
			if (loc-read < toRead-metadataLen) { end = true; }
			toRead = loc-read;
			if (fseek(fp, 0, SEEK_SET) != 0) { perror("fseek() SEEK_SET failed"); return CW_SYS_ERR; }
		} else if (fseek(fp, -read-toRead, SEEK_CUR) != 0) { perror("fseek() SEEK_CUR failed"); return CW_SYS_ERR; }
	}
	if (ferror(fp)) { perror("file error on fread()"); return CW_SYS_ERR; }

	strncpy(resTxid, txid, TXID_CHARS); resTxid[TXID_CHARS] = 0;
	return CW_OK;
}

static CW_STATUS sendFileTreeLayer(FILE *fp, struct CWS_rpc_pack *rp, CW_TYPE cwFType, int depth, int *numTxs, FILE *treeFp, char *resTxid) {
	char hexChunk[TX_DATA_CHARS + 1]; char *hexChunkPtr = hexChunk;
	int metadataLen = depth ? CW_METADATA_CHARS : CW_METADATA_BYTES;
	int dataLen = depth ? TX_DATA_CHARS : TX_DATA_BYTES;
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
				init_CW_file_metadata(&md, cwFType);
				md.depth = depth;
				hexAppendMetadata(hexChunk, &md);
			} else { ++*numTxs; }
		}

		if ((sendTxStatus = sendTx((const char **)&hexChunkPtr, 1, rp, resTxid)) != CW_OK) { return sendTxStatus; }
		++*numTxs;	
		if (fputs(resTxid, treeFp) == EOF) { perror("fputs() failed"); return CW_SYS_ERR; }
	}
	if (ferror(fp)) { perror("file error on fread()"); return CW_SYS_ERR; }

	return CW_OK;
}

static CW_STATUS sendFileTree(FILE *fp, struct CWS_rpc_pack *rp, CW_TYPE cwFType, int maxDepth, int depth, char *resTxid) {
	if (maxDepth >= 0 && depth >= maxDepth) { return sendFileChain(fp, rp, cwFType, depth, resTxid); }

	FILE *tfp;
	if ((tfp = tmpfile()) == NULL) { perror("tmpfile() failed"); return CW_SYS_ERR; }

	CW_STATUS status;

	int numTxs;
	char txidF[TXID_CHARS+1];
	if ((status = sendFileTreeLayer(fp, rp, cwFType, depth, &numTxs, tfp, txidF)) != CW_OK) { goto cleanup; }
	if (numTxs < 2) {
		strncpy(resTxid, txidF, TXID_CHARS);
		resTxid[TXID_CHARS] = 0;
		status = CW_OK;
		goto cleanup;
	}

	rewind(tfp);
	status = sendFileTree(tfp, rp, cwFType, maxDepth, depth+1, resTxid);

	cleanup:
		fclose(tfp);
		return status;
}

static inline CW_STATUS sendFileFromStream(FILE *stream, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, char *resTxid) {
	CW_STATUS status = sendFileTree(stream, rpcPack, params->cwType, params->maxTreeDepth, 0, resTxid);
	fprintf(stderr, "\n");
	return status;
}

static CW_STATUS sendFileFromPath(const char *path, struct CWS_rpc_pack *rpcPack, struct CWS_params *params, char *resTxid) {
	FILE *fp;
	if ((fp = fopen(path, "rb")) == NULL) { perror("fopen() failed"); return CW_SYS_ERR; }	

	CW_STATUS status = sendFileFromStream(fp, rpcPack, params, resTxid);

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

CW_STATUS CWS_send_from_stream(FILE *stream, struct CWS_params *params, double *balanceDiff, char *resTxid) {
	CW_STATUS status;
	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { goto cleanup; }

	double bal;
	if (balanceDiff && (status = checkBalance(&rpcPack, &bal)) != CW_OK) { goto cleanup; }

	status = sendFileFromStream(stream, &rpcPack, params, resTxid);	
	
	double balN;
	if (balanceDiff && (status = checkBalance(&rpcPack, &balN)) == CW_OK) { *balanceDiff = bal - balN; }

	cleanup:
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_from_path(const char *path, struct CWS_params *params, double *balanceDiff, char *resTxid) {
	CW_STATUS status;
	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { goto cleanup; }

	double bal;
	if (balanceDiff && (status = checkBalance(&rpcPack, &bal)) != CW_OK) { goto cleanup; }

	status = sendFileFromPath(path, &rpcPack, params, resTxid);	
	
	double balN;
	if (balanceDiff && (status = checkBalance(&rpcPack, &balN)) == CW_OK) { *balanceDiff = bal - balN; }

	cleanup:
		cleanupRpc(&rpcPack);
		return status;
}

CW_STATUS CWS_send_dir_from_path(const char *path, struct CWS_params *params, double *balanceDiff, char *resTxid) {
	CW_STATUS status;
	struct CWS_rpc_pack rpcPack;
	if ((status = initRpc(params, &rpcPack)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	char const *ftsPathArg[] = { path, NULL };
	int numFiles;
	if ((status = scanDirFileCount(ftsPathArg, &numFiles)) != CW_OK) { cleanupRpc(&rpcPack); return status; }

	// initializing variable-length array before goto statements
	char txidsByteData[numFiles*TXID_BYTES];

	FTS *ftsp;
	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	if ((ftsp = fts_open((char * const *)ftsPathArg, fts_options, NULL)) == NULL) {
		perror("fts_open() failed");
		status = CW_SYS_ERR;
		goto cleanuprpc;
	}
	int pathLen = strlen(path);

	FILE *tmpDirFp;
	if ((tmpDirFp = tmpfile()) == NULL) {
		perror("tmpfile() failed");
		fts_close(ftsp);
		status = CW_SYS_ERR;
		goto cleanuprpc;
	}

	double bal;
	if (balanceDiff && (status = checkBalance(&rpcPack, &bal)) != CW_OK) { goto cleanup; }

	char txid[TXID_CHARS+1];

	int count = 0;
	FTSENT *p;
	while ((p = fts_read(ftsp)) != NULL && count < numFiles) {
		if (p->fts_info == FTS_F && strncmp(p->fts_path, path, pathLen) == 0) {
			fprintf(stderr, "Sending %s", p->fts_path+pathLen);

			if ((status = sendFileFromPath(p->fts_path, &rpcPack, params, txid)) != CW_OK) { goto cleanup; }
			fprintf(stderr, "%s\n", txid);
			if (hexStrToByteArr(txid, 0, txidsByteData+(count*TXID_BYTES)) != TXID_BYTES) {
				fprintf(stderr, "invalid txid from sendFile(); problem with cashsendtools\n");
				status = CW_SYS_ERR;
				goto cleanup;
			}
			++count;

			if (fprintf(tmpDirFp, "%s\n", p->fts_path+pathLen) < 0) {
				perror("fprintf() to tmpDirFp failed");
				status = CW_SYS_ERR;
				goto cleanup;
			}
		}
	}
	if (fprintf(tmpDirFp, "\n") < 0) { perror("fprintf() to tmpDirFp failed"); status = CW_SYS_ERR; goto cleanup; }
	if (fwrite(txidsByteData, TXID_BYTES, numFiles, tmpDirFp) < numFiles) { perror("fwrite() to tmpDirFp failed");
										status = CW_SYS_ERR; goto cleanup; }

	rewind(tmpDirFp);
	fprintf(stderr, "Sending root directory file");
	if ((status = sendFileFromStream(tmpDirFp, &rpcPack, params, resTxid)) != CW_OK) { goto cleanup; }

	double balN;
	if (balanceDiff && (status = checkBalance(&rpcPack, &balN)) == CW_OK) { *balanceDiff = bal - balN; }

	cleanup:
		fclose(tmpDirFp);
		fts_close(ftsp);
	cleanuprpc:
		cleanupRpc(&rpcPack);
		return status;	
}
