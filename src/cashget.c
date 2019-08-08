#include "cashgettools.h"
#include <getopt.h>

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s [FLAGS] <identifier>\n", argv[0]);
		exit(1);
	}

	char *mongodb = NULL;
	char *bitdbNode = BITDB_DEFAULT;

	bool isName = false;
	int revision = CWG_REV_LATEST;

	int c;
	while ((c = getopt(argc, argv, ":lNR:m:b:")) != -1) {
		switch (c) {
			case 'l':
				mongodb = MONGODB_LOCAL_ADDR;
				break;
			case 'N':
				isName = true;
				break;
			
			case 'R':
				revision = atoi(optarg);
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

	char *identifier = argv[optind];
	if (!isName && strlen(identifier) != CW_TXID_CHARS) { fprintf(stderr, "Invalid txid; if getting by nametag, use flag -N\n"); exit(1); }

	struct CWG_params params;
	init_CWG_params(&params, mongodb, bitdbNode, NULL);

	CW_STATUS status;
	if (isName) { status = CWG_get_by_nametag(identifier, revision, &params, STDOUT_FILENO); }
	else { status = CWG_get_by_txid(identifier, &params, STDOUT_FILENO); }

	if (status != CW_OK) { 
		fprintf(stderr, "\nGet failed, error code %d: %s.\n", status, CWG_errno_to_msg(status));
		exit(1);
	}

	return 0;
}
