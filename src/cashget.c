#include <cashgettools.h>
#include <getopt.h>

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

int main(int argc, char **argv) {
	char *mongodb = NULL;
	char *bitdbNode = BITDB_DEFAULT;

	char *getDirPath = NULL;
	bool getDirIndex = false;

	int c;
	while ((c = getopt(argc, argv, ":Dlm:b:")) != -1) {
		switch (c) {	
			case 'D':
				getDirIndex = true;
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
	params.dirPath = getDirPath;

	int getFd = STDOUT_FILENO;
	FILE *dirStream = NULL;
	if (getDirIndex) {
		if ((dirStream = tmpfile()) == NULL) { perror("tmpfile() failed"); exit(1); }	
		getFd = fileno(dirStream);
	}

	CW_STATUS status = CWG_get_by_id(toget, &params, getFd);

	if (status == CW_OK && getDirIndex) {
		rewind(dirStream);
		status = CWG_dirindex_raw_to_json(dirStream, stdout);
	}
	if (dirStream) { fclose(dirStream); }

	if (status != CW_OK) { 
		fprintf(stderr, "\nGet failed, error code %d: %s.\n", status, CWG_errno_to_msg(status));
		exit(1);
	}

	return 0;
}
