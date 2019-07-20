#include "cashsendtools.h"

#define RPC_SERVER_DEFAULT "127.0.0.1"
#define RPC_PORT_DEFAULT 8332
#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"
#define MAX_TREE_DEPTH_DEFAULT -1

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <path> [OPTIONS]\n", argv[0]);
		exit(1);
	}

	struct CWS_params params;
	init_CWS_params(&params, RPC_SERVER_DEFAULT, RPC_PORT_DEFAULT, RPC_USER_DEFAULT, RPC_PASS_DEFAULT);
	
	for (int i=2; i<argc; i++) {
		if (strncmp("--data-dir=", argv[i], 11) == 0) { params.datadir = argv[i]+11; }
		if (strncmp("--rpc-user=", argv[i], 11) == 0) { params.rpcUser = argv[i]+11; }
		if (strncmp("--rpc-pass=", argv[i], 11) == 0) { params.rpcPass = argv[i]+11; }
		if (strncmp("--rpc-server=", argv[i], 13) == 0) { params.rpcServer = argv[i]+13; }
		if (strncmp("--rpc-port=", argv[i], 11) == 0) { params.rpcPort = atoi(argv[i]+11); }
		if (strncmp("--max-tree-depth=", argv[i], 17) == 0) { params.maxTreeDepth = atoi(argv[i]+17); }
		if (strncmp("--mime-set", argv[i], 10) == 0) { params.cwType = CW_T_MIMESET; }
	}

	char *path = argv[1];

	double balanceDiff;
	char txid[CW_TXID_CHARS+1];

	CW_STATUS status;
	if (strcmp(path, "-") == 0) {
		status = CWS_send_from_stream(stdin, &params, &balanceDiff, txid);
	} else {
		struct stat st;
		if (stat(path, &st) != 0) { perror("stat() failed"); exit(1); }
		status = S_ISDIR(st.st_mode) ? CWS_send_dir_from_path(path, &params, &balanceDiff, txid)
					     : CWS_send_from_path(path, &params, &balanceDiff, txid);
	}

	if (status != CW_OK) {	
		fprintf(stderr, "\nSend failed, error code %d: %s.\n", status, CWS_errno_to_msg(status));
		if (status == CW_DATADIR_NO) {
			if (!params.datadir) {
				fprintf(stderr, "Please ensure cashwebtools is properly installed with 'make install', or specify a different data directory with --data-dir=\n");
			} else {
				fprintf(stderr, "Please check that the specified data directory is valid under cashwebtools hierarchy and naming scheme.\n");
			}
		}
		exit(1);
	}

	printf("%s", txid);
	fprintf(stderr, "\nSend cost: %.8f\n", balanceDiff);

	return 0;
}
