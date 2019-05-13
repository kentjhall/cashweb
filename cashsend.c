#include "cashsendtools.h"

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: ./cashsend <file-path>");
		exit(1);
	}
	char *filePath = argv[1];

	FILE *fp;
	if ((fp = fopen(filePath, "rb")) == NULL) { die("fopen() failed"); }	
	char *txid = sendFile(fp);
	fclose(fp);
	printf("%s\n", txid);
	free(txid);
}
