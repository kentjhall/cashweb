#include "cashgettools.h"

#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	char *bitdbNode = BITDB_DEFAULT;

	int opc=0;
	for (int i=1; i<argc; i++) {
		if (strncmp("-bitdb=", argv[i], 7) == 0) { bitdbNode = argv[i]+7; ++opc; }
	}

	char txid[TXID_CHARS+1];
	if (argc-opc > 1) { strcpy(txid, argv[1]); } else { fgets(txid, TXID_CHARS+1, stdin); txid[TXID_CHARS] = 0; sleep(1); }

	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!getFile(txid, bitdbNode, STDOUT_FILENO, NULL)) { 
		fprintf(stderr, "get failed; txid is incorrect, stored file is formatted incorrectly, or BitDB hasn't crawled it yet\n");
		return 1;
	}

	curl_global_cleanup();
	return 0;
}
