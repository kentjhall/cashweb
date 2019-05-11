#include "cashwebtools.h"

static char *txid;

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: ./cashget <txid> [options]\n");
		exit(1);
	}
	txid = argv[1];

	for (int i=2; i<argc; i++) {
		if (strncmp("-bitdb=", argv[i], 7) == 0) { bitdbNode = argv[i]+7; }
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	fetchFileWrite(txid, STDOUT_FILENO);

	curl_global_cleanup();
	return 0;
}