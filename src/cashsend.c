#include "cashsendtools.h"

#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"
#define RPC_SERVER_DEFAULT "127.0.0.1"
#define RPC_PORT_DEFAULT 8332

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file-path> <max-tree-depth, optional> [OPTIONS]\n", argv[0]);
		exit(1);
	}

	char *rpcUser = RPC_USER_DEFAULT;
	char *rpcPass = RPC_PASS_DEFAULT;
	char *rpcServer = RPC_SERVER_DEFAULT;
	unsigned short rpcPort = RPC_PORT_DEFAULT;

	int opc = 0;
	for (int i=2; i<argc; i++) {
		if (strncmp("--rpc-user=", argv[i], 11) == 0) { rpcUser = argv[i]+11; ++opc; }
		if (strncmp("--rpc-pass=", argv[i], 11) == 0) { rpcPass = argv[i]+11; ++opc; }
		if (strncmp("--rpc-server=", argv[i], 13) == 0) { rpcServer = argv[i]+13; ++opc; }
		if (strncmp("--rpc-port=", argv[i], 11) == 0) { rpcPort = atoi(argv[i]+11); ++opc; }
	}

	char *filePath = argv[1];
	int maxTreeDepth = argc-opc < 3 ? -1 : atoi(argv[2]); // -1 signifies no max tree depth

	bitcoinrpc_global_init();
	bitcoinrpc_cl_t *rpcClient = bitcoinrpc_cl_init_params(rpcUser, rpcPass, rpcServer, rpcPort);
	if (!rpcClient) { die("bitcoinrpc_cl_init_params() failed"); };

	double balanceDiff;

	char *txid;
	struct stat st;
	if (stat(filePath, &st) != 0) { die("stat() failed"); }
	txid = S_ISDIR(st.st_mode) ? sendDir(filePath, maxTreeDepth, rpcClient, &balanceDiff) : sendFile(filePath, maxTreeDepth, rpcClient, &balanceDiff);

	printf("%s", txid);
	fprintf(stderr, "\nSend cost: %.8f\n", balanceDiff);

	free(txid);
	bitcoinrpc_cl_free(rpcClient);
	bitcoinrpc_global_cleanup();
	return 0;
}
