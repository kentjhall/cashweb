#include "cashsendtools.h"
#include <getopt.h>

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

	FILE *recoveryStream;
	if ((recoveryStream = tmpfile()) == NULL) { perror("tmpfile() failed"); exit(1); }

	struct CWS_params params;
	init_CWS_params(&params, RPC_SERVER_DEFAULT, RPC_PORT_DEFAULT, RPC_USER_DEFAULT, RPC_PASS_DEFAULT, recoveryStream);
	params.cwType = CW_T_MIMESET;
	
	bool recover = false;
	bool estimate = false;
	for (int i=2; i<argc; i++) {
		if (strncmp("--recover", argv[i], 9) == 0) { recover = true; }
		if (strncmp("--estimate", argv[i], 10) == 0) { estimate = true; }
		if (strncmp("--rpc-user=", argv[i], 11) == 0) { params.rpcUser = argv[i]+11; }
		if (strncmp("--rpc-pass=", argv[i], 11) == 0) { params.rpcPass = argv[i]+11; }
		if (strncmp("--rpc-server=", argv[i], 13) == 0) { params.rpcServer = argv[i]+13; }
		if (strncmp("--rpc-port=", argv[i], 11) == 0) { params.rpcPort = atoi(argv[i]+11); }
		if (strncmp("--max-tree-depth=", argv[i], 17) == 0) { params.maxTreeDepth = atoi(argv[i]+17); }
		if (strncmp("--mimetype-no", argv[i], 10) == 0) { params.cwType = CW_T_FILE; }
		if (strncmp("--no-frag", argv[i], 9) == 0) { params.fragUtxos = 0; }
		if (strncmp("--data-dir=", argv[i], 11) == 0) { params.datadir = argv[i]+11; }
	}

	char recName[strlen(argv[1])+10];
	char *lastSlash = strrchr(argv[1], '/');
	snprintf(recName, sizeof(recName), ".%s.cws", lastSlash ? lastSlash+1 : argv[1]);
	for (int i=0; i<strlen(recName); i++) { if (recName[i] == '/') { recName[i] = '_'; } }
	if (!recover && access(recName, F_OK) == 0) {
		fprintf(stderr, "ERROR: recovery file detected for this file. If there was a failed send, progress can be recovered with --recover;\n"
				        "if not, please delete %s\n", recName);
		fclose(recoveryStream);
		exit(1);
	}
	else if (recover && access(recName, F_OK) == -1) {
		fprintf(stderr, "ERROR: unable to find recovery file %s in current directory\n", recName);
		fclose(recoveryStream);
		exit(1);
	}

	char *path = argv[1];

	double costEstimate = 0;
	size_t *txCountSave = params.fragUtxos == 1 ? &params.fragUtxos : NULL;

	double totalCost = 0;
	double lostCost = 0;
	char txid[CW_TXID_CHARS+1]; txid[0] = 0;

	int exitcode = 0;
	CW_STATUS status = CW_OK;
	if (estimate) {
		if (recover) {
			FILE *recoverySave;
			if ((recoverySave = fopen(recName, "r")) == NULL) { perror("fopen() failed"); exitcode = 1; goto estimatecleanup; }

			fprintf(stderr, "Estimating cost... ");
			if ((status = CWS_estimate_cost_from_recovery_stream(recoverySave, &params, txCountSave, &costEstimate)) != CW_OK) {
				exitcode = 1;
				goto estimatecleanup;
			}	
			printf("%.8f", costEstimate);

			fclose(recoverySave);
		}
		else if (strcmp(path, "-") == 0) {
			fprintf(stderr, "Estimating cost... ");
			if ((status = CWS_estimate_cost_from_stream(stdin, &params, txCountSave, &costEstimate)) != CW_OK) {
				exitcode = 1;
				goto estimatecleanup;
			}	
			printf("%.8f", costEstimate);
		} else {
			fprintf(stderr, "Estimating cost... ");
			if ((status = CWS_estimate_cost_from_path(path, &params, txCountSave, &costEstimate)) != CW_OK) {
				exitcode = 1;
				goto estimatecleanup;
			}	
			printf("%.8f", costEstimate);
		}
		estimatecleanup:
			fclose(recoveryStream);
			if (status != CW_OK) { fprintf(stderr, "Failed to get cost estimate, error code %d: %s.\n", status, CWS_errno_to_msg(status)); }
			return exitcode;
	}
	else {
		if (recover) {
			FILE *recoverySave;
			if ((recoverySave = fopen(recName, "r")) == NULL) { fclose(recoveryStream); perror("fopen() failed"); exit(1); }
		
			status = CWS_send_from_recovery_stream(recoverySave, &params, &totalCost, &lostCost, txid);
			fclose(recoverySave);
			if (status == CW_OK) { fprintf(stderr, "\nRecovery successful; please delete %s", recName); }
		}
		else if (strcmp(path, "-") == 0) {
			status = CWS_send_from_stream(stdin, &params, &totalCost, &lostCost, txid);
		} else {
			status = CWS_send_from_path(path, &params, &totalCost, &lostCost, txid);
		}
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
		
		FILE *recoverySave = NULL;
		if (lostCost < totalCost) {
			fprintf(stderr, "Saving recovery data from cashsendtools...\n");

			if ((recoverySave = fopen(recName, "w")) == NULL) { perror("fopen() failed"); goto endrecovery; }
				
			char buf[1000];
			size_t n = 0;
			rewind(recoveryStream);
			while ((n = fread(buf, 1, sizeof(buf), recoveryStream)) > 0) {
				if (fwrite(buf, 1, n, recoverySave) < n) { perror("fwrite() failed"); goto endrecovery; }
			}
			if (ferror(recoveryStream)) { perror("fread() failed"); goto endrecovery; }

			fprintf(stderr, "Successfully saved %.8f BCH worth of recoverable progress to %s in current directory;\n"
					"to attempt finishing the send, re-send from current directory with flag --recover (e.g. %s %s --recover)\n",
					 totalCost-lostCost, recName, argv[0], argv[1]);
		} else { fprintf(stderr, "No progress is recoverable\n"); }

		endrecovery:
			if (recoverySave) { fclose(recoverySave); }
			exitcode = 1;
	} else { printf("%s", txid); }

	fprintf(stderr, "\n%s cost: %.8f BCH\n", !recover ? "Total" : "Added", totalCost);

	fclose(recoveryStream);
	return exitcode;
}
