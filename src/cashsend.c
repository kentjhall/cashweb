#include "cashsendtools.h"
#include <getopt.h>
#include <ctype.h>

#define RPC_SERVER_DEFAULT "127.0.0.1"
#define RPC_PORT_DEFAULT 8332
#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s [flags] <tosend>\n", argv[0]);
		exit(1);
	}	

	FILE *recoveryStream;
	if ((recoveryStream = tmpfile()) == NULL) { perror("tmpfile() failed"); exit(1); }

	struct CWS_params params;
	init_CWS_params(&params, RPC_SERVER_DEFAULT, RPC_PORT_DEFAULT, RPC_USER_DEFAULT, RPC_PASS_DEFAULT, recoveryStream);
	params.cwType = CW_T_MIMESET;
	
	bool no = false;
	bool revImmutable = false;
	char *nametag = NULL;
	char *revisionUtxo = NULL;
	bool recover = false;
	bool estimate = false;
	int c;
	while ((c = getopt(argc, argv, ":nmfreIN:R:t:d:u:p:a:o:")) != -1) {
		switch (c) {
			case 'n':
				no = true;
				break;
			case 'm':
				params.cwType = no ? CW_T_FILE : CW_T_MIMESET;
				no = false;
				break;
			case 'f':
				params.fragUtxos = no ? 0 : params.fragUtxos;
				no = false;
				break;
			case 'r':
				recover = no ? false : true;
				no = false;
				break;
			case 'e':
				estimate = no ? false : true;
				no = false;
				break;
			case 'I':
				revImmutable = no ? false : true;
				no = false;
				break;
			case 'N':
				nametag = optarg;	
				break;
			case 'R':
				revisionUtxo = optarg;
				break;
			case 't':
				params.maxTreeDepth = atoi(optarg);
				break;
			case 'd':
				params.datadir = optarg;
				break;
			case 'u':
				params.rpcUser = optarg;
				break;
			case 'p':
				params.rpcPass = optarg;
				break;
			case 'a':
				params.rpcServer = optarg;
				break;
			case 'o':
				params.rpcPort = atoi(optarg);
				break;
			case ':':
				fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				exit(1);
			case '?':
				if (isprint(optopt)) {
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				} else {
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);	
				}
				exit(1);
			default:
				fprintf(stderr, "getopt() unknown error\n"); exit(1);
		}
	}

	char *tosend = argv[optind];
	char recName[strlen(tosend)+10];
	char *lastSlash = strrchr(tosend, '/');
	snprintf(recName, sizeof(recName), ".%s.cws", lastSlash ? lastSlash+1 : tosend);
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

	double costEstimate = 0;
	size_t *txCountSave = params.fragUtxos == 1 ? &params.fragUtxos : NULL;

	double totalCost = 0;
	double lostCost = 0;
	char txid[CW_TXID_CHARS+1]; txid[0] = 0;

	int exitcode = 0;
	CW_STATUS status = CW_OK;
	if (estimate) {
		fprintf(stderr, "Estimating cost... ");
		if (nametag) {
			fprintf(stderr, "No estimate available for sending nametag; it's cheap\n");
		}
		else if (revisionUtxo) {
			fprintf(stderr, "No estimate available for sending revision update; it's cheap\n");
		}
		else if (recover) {
			FILE *recoverySave;
			if ((recoverySave = fopen(recName, "r")) == NULL) { perror("fopen() failed"); exitcode = 1; goto estimatecleanup; }

			if ((status = CWS_estimate_cost_from_recovery_stream(recoverySave, &params, txCountSave, &costEstimate)) != CW_OK) {
				exitcode = 1;
				goto estimatecleanup;
			}	
			printf("%.8f", costEstimate);

			fclose(recoverySave);
		}
		else if (strcmp(tosend, "-") == 0) {
			if ((status = CWS_estimate_cost_from_stream(stdin, &params, txCountSave, &costEstimate)) != CW_OK) {
				exitcode = 1;
				goto estimatecleanup;
			}	
			printf("%.8f", costEstimate);
		} else {
			if ((status = CWS_estimate_cost_from_path(tosend, &params, txCountSave, &costEstimate)) != CW_OK) {
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
		if (nametag) {
			status = CWS_send_standard_nametag(nametag, tosend, revImmutable, &params, &totalCost, txid);
		}
		else if (revisionUtxo) {
			status = CWS_send_replace_revision(revisionUtxo, tosend, revImmutable, &params, &totalCost, txid);
		}
		else if (recover) {
			FILE *recoverySave;
			if ((recoverySave = fopen(recName, "r")) == NULL) { fclose(recoveryStream); perror("fopen() failed"); exit(1); }
		
			status = CWS_send_from_recovery_stream(recoverySave, &params, &totalCost, &lostCost, txid);
			fclose(recoverySave);
			if (status == CW_OK) { fprintf(stderr, "\nRecovery successful; please delete %s", recName); }
		}
		else if (strcmp(tosend, "-") == 0) {
			status = CWS_send_from_stream(stdin, &params, &totalCost, &lostCost, txid);
		} else {
			status = CWS_send_from_path(tosend, &params, &totalCost, &lostCost, txid);
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
					 totalCost-lostCost, recName, argv[0], tosend);
		} else { fprintf(stderr, "No progress is recoverable\n"); }

		endrecovery:
			if (recoverySave) { fclose(recoverySave); }
			exitcode = 1;
	} else { printf("%s", txid); }

	fprintf(stderr, "\n%s cost: %.8f BCH\n", !recover ? "Total" : "Added", totalCost);

	fclose(recoveryStream);
	return exitcode;
}
