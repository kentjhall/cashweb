#include "cashgettools.h"

#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <txid> [OPTIONS]\n", argv[0]);
		exit(1);
	}

	char *bitdbNode = BITDB_DEFAULT;

	int opc=0;
	for (int i=1; i<argc; i++) {
		if (strncmp("--bitdb=", argv[i], 8) == 0) { bitdbNode = argv[i]+7; ++opc; }
	}

	char txid[TXID_CHARS+1];
	if (argc-opc > 1) { strcpy(txid, argv[1]); } else { fgets(txid, TXID_CHARS+1, stdin); txid[TXID_CHARS] = 0; sleep(1); }

	curl_global_init(CURL_GLOBAL_DEFAULT);

	int status = CW_OK;
	if ((status = getFile(txid, bitdbNode, NULL, STDOUT_FILENO)) != CW_OK) { 
		fprintf(stderr, "\nGet failed, error code %d:  %s\n", status, errNoToMsg(status));
	}

	curl_global_cleanup();
	return status;
}
