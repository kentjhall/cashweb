#include <cashgettools.h>
#include <getopt.h>

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	char *mongodb = NULL;
	char *bitdbNode = BITDB_DEFAULT;

	bool getInfo = false;
	bool getDirIndex = false;
	
	int c;
	while ((c = getopt(argc, argv, ":Dilm:b:")) != -1) {
		switch (c) {		
			case 'D':
				getDirIndex = true;
				break;
			case 'i':
				getInfo = true;	
				break;
			case 'l':
				mongodb = MONGODB_LOCAL_ADDR;
				break;
			case 'm':
				mongodb = optarg;
				break;
			case 'b':
				bitdbNode = optarg;
				break;
			case ':':
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
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

	if (argc <= optind) {
		fprintf(stderr, "usage: %s [FLAGS] <toget>\n", argv[0]);
		exit(1);
	}

	char *toget = argv[optind];

	struct CWG_params params;
	init_CWG_params(&params, mongodb, bitdbNode, NULL);

	int getFd = STDOUT_FILENO;
	FILE *dirStream = NULL;
	CW_STATUS status;
	if (getInfo) {
		char *name;
		int rev;
		if (CW_is_valid_nametag_id(toget, &rev, &name)) {
			struct CWG_nametag_info info;
			init_CWG_nametag_info(&info);
			if ((status = CWG_get_nametag_info(name, rev, &params, &info)) == CW_OK) {
				if (info.revisionTxid) { printf("Script is mutable.\nRevision UTXO: TXID %s, VOUT %d.\n", info.revisionTxid, CW_REVISION_INPUT_VOUT); }	
				else {
					printf("Script is immutable.\n");
					if (*info.nameRefs) { printf("NOTE: Contains nametag reference(s) which may have mutable scripting.\n"); }
				}
				if (rev < 0) { printf("On revision %d.\n", info.revision); }
				else if (rev > info.revision) { printf("Actual revision %d.\n", info.revision); }
				if (*info.nameRefs) {
					printf("Name(s) referenced: ");
					char **nameRefs = info.nameRefs;
					while (*nameRefs) {
						printf("'%s'", *nameRefs++);
						if (*nameRefs) { printf(", "); }
					}
					printf(".\n");
				}
				if (*info.txidRefs) {
					printf("TXID(s) referenced: ");
					char **txidRefs = info.txidRefs;
					while (*txidRefs) {
						printf("%s", *txidRefs++);
						if (*txidRefs) { printf(", "); }
					}
					printf(".\n");
				}
			}
			destroy_CWG_nametag_info(&info);
			goto end;
		}
		else {
			fprintf(stderr, "Unsupported ID type for getting info.\n");
			exit(1);
		}
	}
	else if (getDirIndex) {
		if ((dirStream = tmpfile()) == NULL) { perror("tmpfile() failed"); exit(1); }	
		getFd = fileno(dirStream);
	}

	status = CWG_get_by_id(toget, &params, getFd);

	if (status == CW_OK && getDirIndex) {
		rewind(dirStream);
		status = CWG_dirindex_raw_to_json(dirStream, stdout);
	}
	if (dirStream) { fclose(dirStream); }

	end:
		if (status != CW_OK) { 
			fprintf(stderr, "\nGet failed, error code %d: %s.\n", status, CWG_errno_to_msg(status));
			exit(1);
		}
		return 0;
}
