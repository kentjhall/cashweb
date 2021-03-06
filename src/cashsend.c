#include <cashsendtools.h>
#include <unistd.h>
#include <getopt.h>

#define USAGE_STR "usage: %s [FLAGS] <tosend>\n"
#define HELP_STR \
	USAGE_STR\
	"\n"\
	" Flag          | Use\n"\
	"---------------|---------------------------------------------------------------------------------------------------------------------------------------------\n"\
	"[none]         | send file at path <tosend> (or from stdin if <tosend> is \"-\") via default RPC settings\n"\
	"-u <ARG>       | specify RPC user (default is '"RPC_USER_DEFAULT"')\n"\
	"-p <ARG>       | specify RPC password (default is '"RPC_PASS_DEFAULT"')\n"\
	"-a <ARG>       | specify RPC server address (default is "RPC_SERVER_DEFAULT")\n"\
	"-o <ARG>       | specify RPC server port (default is "RPC_PORT_DEFAULT")\n"\
	"-d <ARG>       | specify location of valid cashwebtools data directory (default is install directory)\n"\
	"-t <ARG>       | specify max tree depth (i.e., allows for file to be downloaded progressively in chunks, rather than all at once)\n"\
	"-nm            | disable default behavior of interpreting/encoding MIME type when sending from path\n"\
	"-f <ARG>       | specify number of UTXOs to fragment at a time; setting 0 will disable UTXO distribution\n"\
	"-r             | recover prior failed send\n"\
	"-e             | estimate cost of send (a rather loose approximation under current implementation)\n"\
	"-O <ARG>       | when sending from directory path, save index to specified location rather than sending to network\n"\
	"-E             | send as raw directory index (may be product of -O send)\n"\
	"-D             | send as JSON format directory index\n"\
	"-N <ARG>       | send standard nametag with specified name that references valid CashWeb ID <tosend> (i.e. to \"name\" the given identifier)\n"\
	"-R             | send replacement revision that references valid CashWeb ID <tosend>; specify name with -N (i.e. to replace existing identifier for nametag)\n"\
	"-B             | send prepend revision that references valid CashWeb ID <tosend>; specify name with -N\n"\
	"-A             | send append revision that references valid CashWeb ID <tosend>; specify name with -N\n"\
	"-C <ARG>       | send insert revision for specified position that references valid CashWeb ID <tosend>; specify name with -N\n"\
	"-X <ARG>       | send delete revision to delete <tosend> bytes starting at specified position; specify name with -N\n"\
	"-P <ARG> <ARG> | specify path link (used with directory nametag/revision) – first path to link, second path being linked to; <tosend> may be omitted\n"\
	"-I             | used with nametag/revision, script will be verifiably immutable, and revision UTXO will not be locked; <tosend> may be omitted>\n"\
	"-T <ARG>       | used with nametag/revision, will transfer ownership to specified address (recipient must manually lock); <tosend> may be omitted>\n"\
	"-l             | ensure all revision locks are locked for sending with bitcoin-cli (may be necessary after restarting bitcoind); <tosend> may be omitted\n"\
	"-L <ARG>       | lock specified revision txid under name given by <tosend>\n"\
	"-U             | unlock name given by <tosend>\n"

#define RPC_USER_DEFAULT "root"
#define RPC_PASS_DEFAULT "bitcoin"
#define RPC_SERVER_DEFAULT "127.0.0.1"
#define RPC_PORT_DEFAULT "8332"

int main(int argc, char **argv) {	
	int exitcode = 0;	

	struct CWS_params params;
	init_CWS_params(&params, RPC_SERVER_DEFAULT, atoi(RPC_PORT_DEFAULT), RPC_USER_DEFAULT, RPC_PASS_DEFAULT, NULL);
	params.cwType = CW_T_MIMESET;

	struct CWS_revision_pack revPack;
	init_CWS_revision_pack(&revPack);
	
	bool no = false;
	char *name = NULL;
	bool revReplace = false;
	bool revPrepend = false;
	bool revAppend = false;
	ssize_t revInsertPos = -1;
	ssize_t revDeletePos = -1;
	char *lock = NULL;
	bool unlock = false;
	bool justLockUnspents = false;
	bool isDirIndex = false;
	bool recover = false;
	bool estimate = false;
	int c;
	while ((c = getopt(argc, argv, ":hu:p:a:o:d:t:nmf:reO:EDN:RBAC:X:P:IT:lL:U")) != -1) {
		switch (c) {
			case 'h':
				fprintf(stderr, HELP_STR, argv[0]);
				exit(0);
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
			case 'd':
				params.datadir = optarg;
				break;	
			case 't':
				params.maxTreeDepth = atoi(optarg);
				break;
			case 'n':
				no = true;
				break;
			case 'm':
				params.cwType = no ? CW_T_FILE : CW_T_MIMESET;
				no = false;
				break;
			case 'f':
				params.fragUtxos = (size_t)atoi(optarg);
				break;		
			case 'r':
				recover = true;
				break;
			case 'e':
				estimate = true;
				break;
			case 'l':
				justLockUnspents = true;
				break;
			case 'L':
				lock = optarg;	
				break;
			case 'U':
				unlock = true;
				break;
			case 'O':
				params.dirOmitIndex = true;
				params.saveDirStream = fopen(optarg, "w");
				if (!params.saveDirStream) { perror("fopen() failed"); exit(1); }
				break;
			case 'E':
				params.cwType = CW_T_DIR;
				break;
			case 'D':
				isDirIndex = true;
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
				revDeletePos = atoi(optarg);
				break;		
			case 'P':
				revPack.pathToReplace = optarg;
				if (argc <= optind || (argv[optind][0] == '-' && strlen(argv[optind]) > 1)) {
					fprintf (stderr, "Option -%c requires two arguments.\n", optopt);
					exit(1);
				}
				revPack.pathReplacement = argv[optind++];
				break;
			case 'I':
				revPack.immutable = true;
				break;			
			case 'T':
				revPack.transferAddr = optarg;
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
		fprintf(stderr, USAGE_STR"\n-h for help\n", argv[0]);
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

			if (revReplace || revPrepend || revAppend || revInsertPos >= 0 || revDeletePos >= 0 || !is_default_CWS_revision_pack(&revPack)) {
				char revTxid[CW_TXID_CHARS+1]; txid[0] = 0;
				status = CWS_get_stored_revision_txid_by_name(name, &params, revTxid);
				if (status == CW_OK) {
					if (revReplace) { status = CWS_send_replace_revision(revTxid, tosend, &revPack, &params, &totalCost, txid); }
					else if (revPrepend) { status = CWS_send_prepend_revision(revTxid, tosend, &revPack, &params, &totalCost, txid); }
					else if (revAppend) { status = CWS_send_append_revision(revTxid, tosend, &revPack, &params, &totalCost, txid); }
					else if (revInsertPos >= 0) { status = CWS_send_insert_revision(revTxid, revInsertPos, tosend, &revPack, &params, &totalCost, txid); }
					else if (revDeletePos >= 0) { status = CWS_send_delete_revision(revTxid, revDeletePos, atoi(tosend), &revPack, &params, &totalCost, txid);  }
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
			if (params.cwType == CW_T_MIMESET) { params.cwType = CW_T_FILE;	}
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
		if (params.saveDirStream) { fclose(params.saveDirStream); }
		if (dirIndexStream) { fclose(dirIndexStream); }
		return exitcode;
}
