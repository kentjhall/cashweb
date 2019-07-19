#include "cashsendtools.h"

#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"
#define RPC_SERVER_DEFAULT "127.0.0.1"
#define RPC_PORT_DEFAULT 8332
#define MAX_TREE_DEPTH_DEFAULT -1

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <path> [OPTIONS]\n", argv[0]);
		exit(1);
	}

	const char *rpcUser = RPC_USER_DEFAULT;
	const char *rpcPass = RPC_PASS_DEFAULT;
	const char *rpcServer = RPC_SERVER_DEFAULT;
	unsigned short rpcPort = RPC_PORT_DEFAULT;
	int maxTreeDepth = MAX_TREE_DEPTH_DEFAULT;
	CW_TYPE cwType = CW_T_FILE;

	if (CWS_determine_cw_mime_type_by_extension(argv[1], "./hello", &cwType) != CW_OK) { exit(1); }
	fprintf(stderr, "CW Type: %u\n", cwType); exit(1);

	for (int i=2; i<argc; i++) {
		if (strncmp("--rpc-user=", argv[i], 11) == 0) { rpcUser = argv[i]+11; }
		if (strncmp("--rpc-pass=", argv[i], 11) == 0) { rpcPass = argv[i]+11; }
		if (strncmp("--rpc-server=", argv[i], 13) == 0) { rpcServer = argv[i]+13; }
		if (strncmp("--rpc-port=", argv[i], 11) == 0) { rpcPort = atoi(argv[i]+11); }
		if (strncmp("--max-tree-depth=", argv[i], 17) == 0) { maxTreeDepth = atoi(argv[i]+17); }
		if (strncmp("--mime-set", argv[i], 10) == 0) { cwType = CW_T_MIMESET; }
	}

	char *path = argv[1];

	double balanceDiff;
	char txid[CW_TXID_CHARS+1];
	struct CWS_params params;
	init_CWS_params(&params, rpcServer, rpcPort, rpcUser, rpcPass);
	params.maxTreeDepth = maxTreeDepth; params.cwType = cwType;

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
		exit(1);
	}

	printf("%s", txid);
	fprintf(stderr, "\nSend cost: %.8f\n", balanceDiff);

	return 0;
}
