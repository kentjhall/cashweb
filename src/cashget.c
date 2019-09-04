#include <cashgettools.h>
#include <getopt.h>

#define USAGE_STR "usage: %s [FLAGS] <toget>\n"
#define HELP_STR \
	USAGE_STR\
	"\n"\
	" Flag    | Use\n"\
	"---------|-------------------------------------------------------------------------------------------------------------------------\n"\
	"[none]   | get file at valid CashWeb ID <toget> and write to stdout\n"\
	"-b <ARG> | specify BitDB HTTP endpoint URL for querying (default is "BITDB_DEFAULT")\n"\
	"-m <ARG> | specify MongoDB URI for querying\n"\
	"-l       | query MongoDB running locally (equivalent to -m "MONGODB_LOCAL_ADDR")\n"\
	"-d <ARG> | specify location of valid cashwebtools data directory (default is install directory)\n"\
	"-J       | convert valid CashWeb directory index locally stored at location <toget> to readable JSON format and write to stdout\n"\
	"-D       | get CashWeb directory index at valid CashWeb ID <toget>, convert to readable JSON format, and write to stdout\n"\
	"-i       | get info on CashWeb file or nametag by appropriate CashWeb ID <toget>\n"

#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"
#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"

int main(int argc, char **argv) {
	struct CWG_params params;
	init_CWG_params(&params, NULL, BITDB_DEFAULT, NULL);

	bool getInfo = false;
	bool getDirIndex = false;
	bool getDirIndexLocal = false;

	int c;
	while ((c = getopt(argc, argv, ":hb:m:ldJDi")) != -1) {
		switch (c) {			
			case 'h':
				fprintf(stderr, HELP_STR, argv[0]);
				exit(0);
			case 'b':
				params.bitdbNode = optarg;
				break;
			case 'm':
				params.mongodb = optarg;
				break;
			case 'l':
				params.mongodb = MONGODB_LOCAL_ADDR;
				break;	
			case 'd':
				params.datadir = optarg;
				break;
			case 'J':
				getDirIndexLocal = true;
				break;
			case 'D':
				getDirIndex = true;
				break;	
			case 'i':
				getInfo = true;	
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
				fprintf(stderr, "getopt() unknown error\n");
				exit(1);
		}
	}

	if (argc <= optind) {
		fprintf(stderr, USAGE_STR"\n-h for help\n", argv[0]);
		exit(1);
	}

	char *toget = argv[optind];	

	int getFd = STDOUT_FILENO;
	FILE *dirStream = NULL;
	CW_STATUS status;
	if (getInfo) {
		const char *name;
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
		else if (CW_is_valid_txid(toget)) {
			struct CWG_file_info info;
			init_CWG_file_info(&info);
			if ((status = CWG_get_file_info(toget, &params, &info)) == CW_OK) {
				printf("Chain Length: %d\nTree Depth: %d\nType Value: %d%s\nProtocol Version: %d\n\nMIME type: %s\n",
					info.metadata.length, info.metadata.depth,
					info.metadata.type, info.metadata.type == CW_T_DIR ? " (directory index)" : "", info.metadata.pVer,
					info.mimetype[0] ? info.mimetype : "unspecified");
			}
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
	else if (getDirIndexLocal) {
		if ((dirStream = fopen(toget, "rb")) == NULL) { perror("fopen() failed"); exit(1); }
	}

	status = !getDirIndexLocal ? CWG_get_by_id(toget, &params, getFd) : CW_OK;

	if (status == CW_OK && (getDirIndex || getDirIndexLocal)) {
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
