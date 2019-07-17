#include "cashgettools.h"

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	if (argc < 2 || strlen(argv[1]) > TXID_CHARS) {
		fprintf(stderr, "usage: %s <txid> [OPTIONS]\n", argv[0]);
		exit(1);
	}

	char *mongodb = NULL;
	char *bitdbNode = BITDB_DEFAULT;

	for (int i=2; i<argc; i++) {
		if (strncmp("--mongodb=", argv[i], 10) == 0) { mongodb = argv[i]+10; }
		if (strncmp("--mongodb-local", argv[i], 15) == 0) { mongodb = MONGODB_LOCAL_ADDR; }
		if (strncmp("--bitdb=", argv[i], 8) == 0) { bitdbNode = argv[i]+8; }
	}

	char txid[TXID_CHARS+1];
	strncpy(txid, argv[1], TXID_CHARS);
	txid[TXID_CHARS] = 0;

	struct cwGetParams params;
	initCwGetParams(&params, mongodb, bitdbNode);
	CW_STATUS status = CW_OK;
	if ((status = getFile(txid, &params, STDOUT_FILENO)) != CW_OK) { 
		fprintf(stderr, "\nGet failed, error code %d:  %s\n", status, cwgErrNoToMsg(status));
	}

	return status;
}
