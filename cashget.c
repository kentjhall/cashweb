#include "cashgettools.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: ./cashget <txid> [options]\n");
		exit(1);
	}
	char *txid = argv[1];

	for (int i=2; i<argc; i++) {
		if (strncmp("-bitdb=", argv[i], 7) == 0) { bitdbNode = argv[i]+7; }
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!getFile(txid, STDOUT_FILENO)) { 
		fprintf(stderr, "get failed; maybe txid is incorrect, or stored file is formatted incorrectly\n");
		return 1;
	}

	curl_global_cleanup();
	return 0;
}
