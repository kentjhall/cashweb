#include "cashsendtools.h"

#define SEND_DATA_CMD "./send_data_tx.sh"
#define CLI_LINE_BUF 10
#define CHECK_UTXOS_CMD "bitcoin-cli listunspent | jq \'. | length\'"
#define CHECK_BALANCE_CMD "echo $(bc -l <<< \"$(bitcoin-cli getbalance) + $(bitcoin-cli getunconfirmedbalance)\")"
#define ERR_WAIT_CYCLE 5

#define RPC_M_GETBALANCE 0
#define RPC_M_GETUNCONFIRMEDBALANCE 1
#define RPC_METHODS_COUNT 2

#define EXTRA_PUSHDATA_BYTE_THRESHOLD 75

static bitcoinrpc_cl_t *rpcClient = NULL;
static struct bitcoinrpc_method *rpcMethods[RPC_METHODS_COUNT] = { NULL };

static long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

/* static int txDataSize(const char *hexData) { */
/* 	int dataSize = strlen(hexData)/2; */
/* 	return dataSize > EXTRA_PUSHDATA_BYTE_THRESHOLD ? dataSize+2 : dataSize+1; */ 
/* } */

static void initRpcMethods() {
	for (int i=0; i<RPC_METHODS_COUNT; i++) {
		BITCOINRPC_METHOD method;
		switch (i) {
			case RPC_M_GETBALANCE:
				method = BITCOINRPC_METHOD_GETBALANCE;
				break;
			case RPC_M_GETUNCONFIRMEDBALANCE:
				method = BITCOINRPC_METHOD_GETUNCONFIRMEDBALANCE;
				break;
			default:
				return;
		}

		if (rpcMethods[i] == NULL) { rpcMethods[i] = bitcoinrpc_method_init(method); }
		if (!rpcMethods[i]) { fprintf(stderr, "bitcoinrpc_method_init() failed for rpc method %d\n", i); die(NULL); }
	}
}

static void freeRpcMethods() {
	for (int i=0; i<RPC_METHODS_COUNT; i++) { if (rpcMethods[i]) { bitcoinrpc_method_free(rpcMethods[i]); } }
}

static void rpcCall(int rpcMethodI, json_t **jsonResult) {
	bitcoinrpc_resp_t *bResponse = bitcoinrpc_resp_init();
	if (!bResponse) { die("bitcoinrpc_resp_init() failed"); }
	bitcoinrpc_err_t bError;

	struct bitcoinrpc_method *rpcMethod = rpcMethods[rpcMethodI];
	if (!rpcMethod) { fprintf(stderr, "rpc method (identifier %d) not initialized; probably an issue with cashsendtools", rpcMethodI);
			  die(NULL); }
	bitcoinrpc_call(rpcClient, rpcMethod, bResponse, &bError);	

	if (bError.code != BITCOINRPCE_OK) { fprintf(stderr, "rpc error %d [%s]\n", bError.code, bError.msg); die("rpc fail"); }

	json_t *jsonResponse = bitcoinrpc_resp_get(bResponse);
	*jsonResult = json_object_get(jsonResponse, "result");
	json_incref(*jsonResult);
	json_decref(jsonResponse);

	bitcoinrpc_resp_free(bResponse);
}

static int checkUtxos() {
	FILE *sfp;
	if ((sfp = popen(CHECK_UTXOS_CMD, "r")) == NULL) { die("popen() failed"); }
	char utxosStr[CLI_LINE_BUF+1];
	fgets(utxosStr, sizeof(utxosStr), sfp);
	if (ferror(sfp)) { die("popen() file error"); }
	pclose(sfp);
	return atoi(utxosStr);
}

static double checkBalance() {
	json_t *jsonResult;

	rpcCall(RPC_M_GETBALANCE, &jsonResult);
	double balance = json_number_value(jsonResult);
	json_decref(jsonResult);

	rpcCall(RPC_M_GETUNCONFIRMEDBALANCE, &jsonResult);
	balance += json_number_value(jsonResult);
	json_decref(jsonResult);
	
	return balance;
}

static void fileBytesToHexStr(char *hex, const char *fileBytes, int n) {
	for (int i=0; i<n; i++) {
		hex[i*2]   = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) >> 4];
		hex[i*2+1] = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) & 0x0F];
	}
	hex[n*2] = 0;
}

static int sendTxAttempt(char **txid, const char *hexData) {
	FILE *sfp;
	char cmd[strlen(SEND_DATA_CMD)+1+strlen(hexData)+1];
	(*txid)[TXID_CHARS] = 0;
	snprintf(cmd, sizeof(cmd), "%s %s", SEND_DATA_CMD, hexData);
	if ((sfp = popen(cmd, "r")) == NULL) { die("popen() failed"); }
	while (fgets(*txid, TXID_CHARS+1, sfp)) { if (ferror(sfp)) { die("popen() file error"); } }
	pclose(sfp);
	if (strstr(*txid, " ")) { return 0; }
	fprintf(stderr, "-");
	return 1;
}

static void sendTx(char **txid, const char *hexData) {
	int printed;
	while (!sendTxAttempt(txid, hexData)) { 
		printed = 0;
		if (strstr(*txid, "Insufficient funds")) {
			/* double balance = checkBalance(); */	
			/* while (checkBalance() <= balance) { */
			/* 	if (!printed) { fprintf(stderr, "\nInsufficient balance, send more funds..."); printed = 1; } */
			/* 	sleep(ERR_WAIT_CYCLE); */
			/* 	sendTxAttempt(txid, hexData); */	
			/* 	fprintf(stderr, "."); */
			/* } */
		}
		else if (strstr(*txid, "too-long-mempool-chain")) {
			while (checkUtxos() < 1) {
				if (!printed) { fprintf(stderr, "\nWaiting on confirmations..."); printed = 1; }
				sleep(ERR_WAIT_CYCLE);
				sendTxAttempt(txid, hexData);
				fprintf(stderr, ".");
			}
		}
		else if (strstr(*txid, "insufficient priority")) { continue; }
		if (!printed) { fprintf(stderr, "\ntx send error: %s\n", *txid); exit(1); }
		fprintf(stderr, "\n");
	}
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
		sendTx(&txid, hexChunk);
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
		sendTx(&txid, hexChunk);
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
	if (rpcClient == NULL) { rpcClient = rpcCli; }
	initRpcMethods();

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
	if (rpcClient == NULL) { rpcClient = rpcCli; }
	initRpcMethods();

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
