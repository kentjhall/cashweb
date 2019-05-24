#include "cashsendtools.h"

static long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

static int sendTxAttempt(char **txid, const char *hexData) {
	FILE *sfp;
	char cmd[strlen(SEND_DATA_CMD)+1+strlen(hexData)+1];
	(*txid)[TXID_CHARS] = 0;
	snprintf(cmd, sizeof(cmd), "%s %s", SEND_DATA_CMD, hexData);
	if ((sfp = popen(cmd, "r")) == NULL) { die("popen() failed"); }
	while (fgets(*txid, TXID_CHARS+1, sfp)) { if (ferror(sfp)) { die("popen() file error"); } }
	pclose(sfp);
	if (strstr(*txid, "error message:")) { return 0; }
	fprintf(stderr, "-");
	return 1;
}

static int checkUtxos() {
	FILE *sfp;
	if ((sfp = popen(CHECK_UTXOS_CMD, "r")) == NULL) { die("popen() failed"); }
	char utxosStr[10];
	fgets(utxosStr, sizeof(utxosStr), sfp);
	if (ferror(sfp)) { die("popen() file error"); }
	pclose(sfp);
	return atoi(utxosStr);
}

static void sendTx(char **txid, const char *hexData) {
	int printed;
	while (!sendTxAttempt(txid, hexData)) { 
		printed = 0;
		if (strstr(*txid, "too-long-mempool-chain")) {
			while (checkUtxos() < 1) {
				if (!printed) { fprintf(stderr, "\nWaiting on confirmations..."); printed = 1; }
				sleep(WAIT_CYCLE);
				sendTxAttempt(txid, hexData);
				fprintf(stderr, ".");
			}
		}
		if (!printed) { fprintf(stderr, "\nbitcoin-cli error: %s\n", *txid); exit(1); }
		fprintf(stderr, "\n");
	}
}

static void fileBytesToHexStr(char *hex, const char *fileBytes, int n) {
	for (int i=0; i<n; i++) {
		hex[i*2]   = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) >> 4];
		hex[i*2+1] = "0123456789ABCDEF"[((uint8_t)fileBytes[i]) & 0x0F];
	}
	hex[n*2] = 0;
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
		if (fputs(txid, tempfp) < 0) { die("fputs() failed"); }
	}
	if (fputs(TREE_SUFFIX, tempfp) < 0) { die("fputs() failed"); }
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
	if (depth >= maxDepth) { return sendFileChain(fp, depth); }

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

char *sendFile(FILE *fp, int maxTreeDepth) {
	return sendFileTree(fp, 0, maxTreeDepth);
}
