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
	*txidPtr[TXID_CHARS] = 0;
	while (fgets(*txidPtr, TXID_CHARS+1, sfp)) { if (ferror(sfp)) { die("popen() file error"); } }
	pclose(sfp);
}

static char *fileToHexToTx(FILE *fp, FILE *tempfp, int numTxids, int isHex) {
	char *hexChunk;
	if ((hexChunk = malloc(TX_DATA_CHARS + 1)) == NULL) { die("malloc failed"); }
	char *hexChunkIt;
	char *txid;
	if ((txid = malloc(TXID_CHARS+1)) == NULL) { die("malloc failed"); }
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
		sendTx(hexChunk, &txid);
		if (fputs(txid, tempfp) < 0) { die("fputs() failed"); }
		i++;
	}
	free(hexChunk);
	return txid;
}

static char *sendFileR(FILE *fp, int isHex) {
	int numTxids = (int)ceil((double)fileSize(fileno(fp))/(isHex ? TX_DATA_CHARS : TX_DATA_BYTES));
	
	FILE *tfp;
	if ((tfp = tmpfile()) == NULL) { die("tmpfile() failed"); }

	char *txidF = fileToHexToTx(fp, tfp, numTxids, isHex);
	if (numTxids < 2) { fclose(tfp); return txidF; }
	free(txidF);

	rewind(tfp);
	char *txid = sendFileR(tfp, 1);
	fclose(tfp);
	return txid;
}

char *sendFile(FILE *fp) {
	return sendFileR(fp, 0);
}
