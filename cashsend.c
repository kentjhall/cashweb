#include "cashsendtools.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file-path> <max-tree-depth, optional>\n", argv[0]);
		exit(1);
	}
	char *filePath = argv[1];
	int maxTreeDepth = argc < 3 ? -1 : atoi(argv[2]); // -1 signifies no max tree depth

	double balanceStart = checkBalance();
	FILE *fp;
	if ((fp = fopen(filePath, "rb")) == NULL) { die("fopen() failed"); }	
	char *txid = sendFile(fp, maxTreeDepth);
	fclose(fp);
	printf("%s\n", txid);
	fprintf(stderr, "\n");
	fprintf(stderr, "Send cost: %f\n", balanceStart-checkBalance());
	free(txid);
}