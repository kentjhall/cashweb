#include "cashgettools.h"
#include <getopt.h>

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s [FLAGS] <toget>\n", argv[0]);
		exit(1);
	}

	char *mongodb = NULL;
	char *bitdbNode = BITDB_DEFAULT;

	bool isName = false;
	char *pathGetId = NULL;
	int revision = CWG_REV_LATEST;

	int c;
	while ((c = getopt(argc, argv, ":NR:p:lm:b:")) != -1) {
		switch (c) {	
			case 'N':
				isName = true;
				break;
			case 'R':
				revision = atoi(optarg);
				break;
			case 'p':
				pathGetId = optarg;
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

	char *toget = argv[optind];
	if (!isName && strlen(toget) != CW_TXID_CHARS) { fprintf(stderr, "Invalid txid; if getting by nametag, use flag -N\n"); exit(1); }

	struct CWG_params params;
	init_CWG_params(&params, mongodb, bitdbNode, NULL);

	int getFd = STDOUT_FILENO;
	FILE *dirStream = NULL;
	if (pathGetId) {
		if ((dirStream = tmpfile()) == NULL) { perror("tmpfile() failed"); exit(1); }	
		getFd = fileno(dirStream);
	}

	CW_STATUS status;
	if (isName) { status = CWG_get_by_nametag(toget, revision, &params, getFd); }
	else { status = CWG_get_by_txid(toget, &params, getFd); }

	if (status == CW_OK && pathGetId) {
		rewind(dirStream);
		char id[CW_NAME_MAX_LEN+1]; id[0] = 0;
		if ((status = CWG_dirindex_path_to_identifier(dirStream, pathGetId, id)) == CW_OK) {
			printf("%s", id);
		}
	}
	if (dirStream) { fclose(dirStream); }

	if (status != CW_OK) { 
		fprintf(stderr, "\nGet failed, error code %d: %s.\n", status, CWG_errno_to_msg(status));
		exit(1);
	}

	return 0;
}
