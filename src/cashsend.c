#include <cashsendtools.h>
#include <getopt.h>

#define RPC_SERVER_DEFAULT "127.0.0.1"
#define RPC_PORT_DEFAULT 8332
#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"

int main(int argc, char **argv) {	
	int exitcode = 0;	

	struct CWS_params params;
	init_CWS_params(&params, RPC_SERVER_DEFAULT, RPC_PORT_DEFAULT, RPC_USER_DEFAULT, RPC_PASS_DEFAULT, NULL);
	params.cwType = CW_T_MIMESET;

	struct CWS_revision_pack revPack;
	init_CWS_revision_pack(&revPack);
	
	bool no = false;
	char *name = NULL;
	bool revReplace = false;
	bool revPrepend = false;
	bool revAppend = false;
	size_t revInsertPos = 0;
	size_t revDeleteBytes = 0;
	char *lock = NULL;
	bool unlock = false;
	bool justLockUnspents = false;
	bool isDirIndex = false;
	bool recover = false;
	bool estimate = false;
	int c;
	while ((c = getopt(argc, argv, ":nmfrelDIN:RBAC:X:T:P:L:Ut:d:u:p:a:o:")) != -1) {
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
			case 'l':
				justLockUnspents = no ? false : true;
				no = false;
				break;
			case 'D':
				isDirIndex = no ? false : true;
				no = false;
				break;
			case 'I':
				revPack.immutable = no ? false : true;
				no = false;
				break;
			case 'N':
				name = optarg;	
				break;
			case 'R':
				revReplace = true;
				break;
			case 'B':
				revPrepend = true;
				break;
			case 'A':
				revAppend = true;
				break;
			case 'C':
				revInsertPos = atoi(optarg);
				break;
			case 'X':
				revDeleteBytes = atoi(optarg);
				break;
			case 'T':
				revPack.transferAddr = optarg;
				break;
			case 'P':
				revPack.pathToReplace = optarg;
				if (argc <= optind || (argv[optind][0] == '-' && strlen(argv[optind]) > 1)) {
					fprintf (stderr, "Option -%c requires two arguments.\n", optopt);
					exit(1);
				}
				revPack.pathReplacement = argv[optind++];
				break;
			case 'L':
				lock = optarg;	
				break;
			case 'U':
				unlock = no ? false : true;
				no = false;
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
				fprintf(stderr, "getopt() unknown error\n");
				exit(1);
		}
	}

	if (argc <= optind && !justLockUnspents && is_default_CWS_revision_pack(&revPack)) {
		fprintf(stderr, "usage: %s [FLAGS] <tosend>\n", argv[0]);
		exit(1);
	}	

	FILE *recoveryStream;
	if ((recoveryStream = tmpfile()) == NULL) { perror("tmpfile() failed"); exit(1); }
	params.recoveryStream = recoveryStream;

	FILE *dirIndexStream = NULL;
	if (isDirIndex && (dirIndexStream = tmpfile()) == NULL) { perror("tmpfile() failed"); fclose(recoveryStream); exit(1); }

	char *tosend = argc > optind ? argv[optind] : "";
	bool fromStdin = strcmp(tosend, "-") == 0;

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

	CW_STATUS status = CW_OK;
	if (estimate) {
		fprintf(stderr, "Estimating cost... ");
		if (lock || unlock || justLockUnspents) {
			fprintf(stderr, "Bad call.\n");
		}
		else if (name) {
			fprintf(stderr, "No estimate available for naming/revisioning; it's pretty cheap.\n");
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
		else if (isDirIndex) {
			FILE *src;
			if (fromStdin) { src = stdin; }
			else if ((src = fopen(tosend, "rb")) == NULL) { perror("fopen() failed"); exitcode = 1; goto cleanup; }

			status = CWS_dirindex_json_to_raw(src, dirIndexStream);
			if (!fromStdin) { fclose(src); }
			if (status != CW_OK) { exitcode = 1; goto estimatecleanup; }

			rewind(dirIndexStream);
			params.cwType = CW_T_DIR;
			if ((status = CWS_estimate_cost_from_stream(dirIndexStream, &params, txCountSave, &costEstimate)) != CW_OK) {
				exitcode = 1;
				goto estimatecleanup;
			}	
			printf("%.8f", costEstimate);
		}
		else if (fromStdin) {
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
			if (status != CW_OK) { fprintf(stderr, "\nFailed to get cost estimate, error code %d: %s.\n", status, CWS_errno_to_msg(status)); }
			goto cleanup;
	} else {
		if (lock || unlock) {
			status = CWS_set_revision_lock(tosend, lock, unlock, &params);
		}
		else if (justLockUnspents) {
			status = CWS_wallet_lock_revision_utxos(&params);
		}	
		else if (name) {
			char id[CW_NAMETAG_ID_MAX_LEN+1];
			if (fromStdin) {
				if (fgets(id, sizeof(id), stdin) == NULL) { perror("fgets() stdin failed"); status = CW_SYS_ERR; goto cleanup; }
				tosend = id;
			}

			if (revReplace || revPrepend || revAppend || revInsertPos > 0 || revDeleteBytes > 0 || !is_default_CWS_revision_pack(&revPack)) {
				char revTxid[CW_TXID_CHARS+1]; txid[0] = 0;
				status = CWS_get_stored_revision_txid_by_name(name, &params, revTxid);
				if (status == CW_OK) {
					if (revReplace) { status = CWS_send_replace_revision(revTxid, tosend, &revPack, &params, &totalCost, txid); }
					else if (revPrepend) { status = CWS_send_prepend_revision(revTxid, tosend, &revPack, &params, &totalCost, txid); }
					else if (revAppend) { status = CWS_send_append_revision(revTxid, tosend, &revPack, &params, &totalCost, txid); }
					else if (revInsertPos > 0) { status = CWS_send_insert_revision(revTxid, revInsertPos, tosend, &revPack, &params, &totalCost, txid); }
					else if (revDeleteBytes > 0) { status = CWS_send_delete_revision(revTxid, atoi(tosend), revDeleteBytes, &revPack, &params, &totalCost, txid);  }
					else if (!is_default_CWS_revision_pack(&revPack)) { status = CWS_send_empty_revision(revTxid, &revPack, &params, &totalCost, txid); }
					else { fprintf(stderr, "Unexpected behavior; problem with cashsend.c"); status = CW_SYS_ERR; }
				} else if (status == CW_CALL_NO) {
					fprintf(stderr, "Specified name not found in revision locks; check %s in data directory", CW_DATADIR_REVISIONS_FILE);
				}
			}
			else {
				status = CWS_send_standard_nametag(name, tosend, &revPack, &params, &totalCost, txid);
			}
			lostCost = totalCost;
		}
		else if (recover) {
			FILE *recoverySave;
			if ((recoverySave = fopen(recName, "r")) == NULL) { fclose(recoveryStream); perror("fopen() failed"); exit(1); }
		
			status = CWS_send_from_recovery_stream(recoverySave, &params, &totalCost, &lostCost, txid);
			fclose(recoverySave);
			if (status == CW_OK) { fprintf(stderr, "\nRecovery successful; please delete %s", recName); }
		}
		else if (isDirIndex) {
			FILE *src;
			if (fromStdin) { src = stdin; }
			else if ((src = fopen(tosend, "rb")) == NULL) { perror("fopen() failed"); exitcode = 1; goto cleanup; }

			status = CWS_dirindex_json_to_raw(src, dirIndexStream);
			if (!fromStdin) { fclose(src); }
			if (status != CW_OK) { exitcode = 1; goto cleanup; }

			rewind(dirIndexStream);
			params.cwType = CW_T_DIR;
			status = CWS_send_from_stream(dirIndexStream, &params, &totalCost, &lostCost, txid);
		}
		else if (fromStdin) {
			params.cwType = CW_T_FILE;	
			status = CWS_send_from_stream(isDirIndex ? dirIndexStream : stdin, &params, &totalCost, &lostCost, txid);
		} else {
			status = CWS_send_from_path(tosend, &params, &totalCost, &lostCost, txid);
		}
	}

	if (status != CW_OK) {	
		fprintf(stderr, "\nAction failed, error code %d: %s.\n", status, CWS_errno_to_msg(status));
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
	}
	if (lock || unlock || justLockUnspents) {
		if (status == CW_OK) { fprintf(stderr, "Success.\n"); }
		goto cleanup;
	}

	if (status == CW_OK) { printf("%s", txid); }

	fprintf(stderr, "\n%s cost: %.8f BCH\n", !recover ? "Total" : "Added", totalCost);

	cleanup:
		fclose(recoveryStream);
		if (dirIndexStream) { fclose(dirIndexStream); }
		return exitcode;
}
