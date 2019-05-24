#include "cashsendtools.h"

#define DEFAULT_MAX_TREE_DEPTH 1

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file-path> <max-tree-depth, default=%d>", argv[0], DEFAULT_MAX_TREE_DEPTH);
		exit(1);
	}
	char *filePath = argv[1];
	int maxTreeDepth = argc < 3 ? DEFAULT_MAX_TREE_DEPTH : atoi(argv[2]);

	FILE *fp;
	if ((fp = fopen(filePath, "rb")) == NULL) { die("fopen() failed"); }	
	char *txid = sendFile(fp, maxTreeDepth);
	fclose(fp);
	printf("%s", txid);
	fprintf(stderr, "\n");
	free(txid);
}
