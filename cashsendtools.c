#include "cashsendtools.h"

static long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

static void sendTx(char *hexData, char **txidPtr) {
	FILE *sfp;
	char cmd[strlen(SEND_DATA_CMD)+1+strlen(hexData)+1];
	strcpy(cmd, SEND_DATA_CMD" ");
	strcat(cmd, hexData);
	if ((sfp = popen(cmd, "r")) == NULL) { die("popen() failed"); }
	(*txidPtr)[TXID_CHARS] = 0;
	while (fgets(*txidPtr, TXID_CHARS+1, sfp)) { if (ferror(sfp)) { die("popen() file error"); } }
	pclose(sfp);
	fprintf(stderr, ".");
}

static char *fileHandleByTxTree(FILE *fp, FILE *tempfp, int depth) {
	char *hexChunk;
	if ((hexChunk = malloc(TX_DATA_CHARS + 1)) == NULL) { die("malloc failed"); }
	char *hexChunkIt;
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	char buf[depth ? TX_DATA_CHARS : TX_DATA_BYTES];
	int n;
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
		if (!depth) {
			hexChunkIt = hexChunk;
			for (int i=0; i<n; i++) {
				snprintf(hexChunkIt, 2+1, "%02x", buf[i]);
				hexChunkIt += 2;
			}
		} else if (n%2 == 0) { 
			memcpy(hexChunk, buf, n); 
			hexChunk[n] = 0; 
		} else { die("fread() error"); }
		sendTx(hexChunk, &txid);
		if (fputs(txid, tempfp) < 0) { die("fputs() failed"); }
	}
	if (ferror(fp)) { die("file error on fread()"); }
	free(hexChunk);
	return txid;
}

static char *fileHandleByTxChain(FILE *fp, long size, int isHex) { 
	char *hexChunk;
	if ((hexChunk = malloc(TX_DATA_CHARS + 1)) == NULL) { die("malloc failed"); }
	char *hexChunkIt;
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	memset(txid, 0, TXID_CHARS);
	char buf[isHex ? TX_DATA_CHARS-TXID_CHARS : TX_DATA_BYTES-TXID_BYTES];
	int toRead = size < sizeof(buf) ? size : sizeof(buf);
	int atEnd = 0;
	long loc;
	if (fseek(fp, -toRead, SEEK_END) != 0) { die("fseek() failed"); }
	while (fread(buf, toRead, 1, fp) > 0) {
		if (!isHex) {
			hexChunkIt = hexChunk;
			for (int i=0; i<toRead; i++) {
				snprintf(hexChunkIt, 2+1, "%02x", buf[i]);
				hexChunkIt += 2;
			}
			strcpy(hexChunkIt, txid);
		} else {
			memcpy(hexChunk, buf, toRead);
			hexChunk[toRead] = 0;
			strcat(hexChunk, txid);
		}
		sendTx(hexChunk, &txid);
		if (atEnd) { break; }
		if ((loc = ftell(fp)) < 2*toRead) { toRead = loc - toRead; fseek(fp, 0, SEEK_SET); atEnd = 1; } else {
			if (fseek(fp, -2*toRead, SEEK_CUR) != 0) { die("fseek() failed"); }
		}
	}
	if (ferror(fp)) { die("file error on fread()"); }
	free (hexChunk);
	return txid;
}

static inline char *sendFileChain(FILE *fp, int isHex) {
	return fileHandleByTxChain(fp, fileSize(fileno(fp)), isHex);
}

static char *sendFileTree(FILE *fp, int depth, int maxDepth, char * (*maxDepthHandler)(FILE *, int)) {
	if (depth > maxDepth) { return maxDepthHandler(fp, depth); }

	FILE *tfp;
	if ((tfp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	char *txidF = fileHandleByTxTree(fp, tfp, depth);
	if ((int)ceil((double)fileSize(fileno(fp))/(depth ? TX_DATA_CHARS : TX_DATA_BYTES)) < 2) { fclose(tfp); return txidF; }
	free(txidF);

	rewind(tfp);
	char *txid = sendFileTree(tfp, depth+1, maxDepth, maxDepthHandler);
	fclose(tfp);
	return txid;
}

char *sendFile(FILE *fp) {
	return sendFileTree(fp, 0, 1, sendFileChain);
}
