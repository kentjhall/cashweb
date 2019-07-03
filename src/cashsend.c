#include "cashsendtools.h"

#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file-path> <max-tree-depth, optional>\n", argv[0]);
		exit(1);
	}

	char *rpcUser = RPC_USER_DEFAULT;
	char *rpcPass = RPC_PASS_DEFAULT;

	for (int i=2; i<argc; i++) {
		if (strncmp("--rpc-user=", argv[i], 11) == 0) { rpcUser = argv[i]+10; }
		if (strncmp("--rpc-pass=", argv[i], 11) == 0) { rpcPass = argv[i]+10; }
	}

	char *filePath = argv[1];
	int maxTreeDepth = argc < 3 ? -1 : atoi(argv[2]); // -1 signifies no max tree depth

	bitcoinrpc_global_init();
	/* rpc_client = */ 

	double balanceStart = checkBalance();

	char *txid;
	struct stat st;
	if (stat(filePath, &st) != 0) { die("stat() failed"); }
	txid = S_ISDIR(st.st_mode) ? sendDir(filePath, maxTreeDepth) : sendFile(filePath, maxTreeDepth);

	printf("%s", txid);
	fprintf(stderr, "\n");
	fprintf(stderr, "Send cost: %f\n", balanceStart-checkBalance());
	free(txid);
}
