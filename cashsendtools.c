#include "cashsendtools.h"

static long fileSize(int fd) {
	struct stat st;
	if (fstat(fd, &st) != 0) { return -1; }
	return st.st_size;
}

static char *sendTx(char *hexData) {
	FILE *sfp;
	char cmd[strlen(SEND_DATA_CMD)+1+strlen(hexData)+1];
	strcpy(cmd, SEND_DATA_CMD" ");
	strcat(cmd, hexData);
	if ((sfp = popen(cmd, "r")) == NULL) { die("popen() failed"); }
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
	txid[TXID_CHARS] = 0;
	while (fgets(txid, TXID_CHARS+1, sfp)) { if (ferror(sfp)) { die("popen() file error"); } }
	pclose(sfp);
	return txid;
}

static void fileToHexHandleByTx(FILE *fp, char **txids, int numTxids, int isHex) {
	char *hexChunk;
	if ((hexChunk = malloc(TX_DATA_CHARS + 1)) == NULL) { die("malloc failed"); }
	char *hexChunkIt;
	char buf[isHex ? TX_DATA_CHARS : TX_DATA_BYTES];
	int i = 0;
	int n;
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0 && i < numTxids) {
		
		if (!isHex) {
			hexChunkIt = hexChunk;
			for (int i=0; i<n; i++) {
				snprintf(hexChunkIt, 2+1, "%02x", buf[i]);
				hexChunkIt += 2;
			}
		} else if (n%2 == 0) { memcpy(hexChunk, buf, n); hexChunk[n] = 0; } else { die("fread() error"); }
		txids[i++] = sendTx(hexChunk);
	}
	free(hexChunk);
}

static char *sendFileR(FILE *fp, int isHex) {
	int numTxids = (int)ceil((double)fileSize(fileno(fp))/(isHex ? TX_DATA_CHARS : TX_DATA_BYTES));
	char **txids = malloc(sizeof(char *) * numTxids);
	fileToHexHandleByTx(fp, txids, numTxids, isHex);

	if (numTxids < 2) { return txids[0]; }

	FILE *tfp;
	if ((tfp = tmpfile()) == NULL) { die("tmpfile() failed"); }
	for (int i=0; i<numTxids; i++) {
		if (fputs(txids[i], tfp) < 0) { die("fputs() failed"); }
		free(txids[i]);
	}
	free(txids);

	rewind(tfp);
	char *txid = sendFileR(tfp, 1);
	fclose(tfp);
	return txid;
}

char *sendFile(FILE *fp) {
	return sendFileR(fp, 0);
}
