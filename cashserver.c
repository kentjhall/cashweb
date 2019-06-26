#include "cashgettools.h"
#include <arpa/inet.h>
#include <microhttpd.h>

#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"
#define DIR_LINE_BUF 1000

static char *bitdbNode;

static void cashFoundHandler(int found, int sockfd) {
	const char *respStatus = found ? "HTTP/1.1 200 OK\r\n\r\n" : "HTTP/1.1 404 Not Found\r\n\r\n<html><body><h1>Error 404</h1></body></html>\r\n";
	if (send(sockfd, respStatus, strlen(respStatus), 0) != strlen(respStatus)) { fprintf(stderr, "send() failed on response\n"); }
}

static int requestHandler(void *cls,
			  struct MHD_Connection *connection,
			  const char *url,
			  const char *method,
			  const char *version,
			  const char *upload_data,
			  size_t *upload_data_size,
			  void **ptr) {
	static int dummy;
	if (strcmp(method, "GET") != 0) { return MHD_NO;  }
	if (*ptr != &dummy) { *ptr = &dummy; return MHD_YES; }
	if (*upload_data_size != 0) { return MHD_NO; }

	const union MHD_ConnectionInfo *info_fd = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CONNECTION_FD);
	const union MHD_ConnectionInfo *info_addr = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	char *clntip = inet_ntoa(((struct sockaddr_in *) info_addr->client_addr)->sin_addr);
	fprintf(stderr, "%s: requested %s\n", clntip, url);

	char *realUrl;
	if ((realUrl = strstr(url+1, "/")) != NULL) {
		if (strcmp(realUrl, "/") == 0) { realUrl = "/index.html"; }
		char txid[TXID_CHARS+1];
		strncpy(txid, url+1, TXID_CHARS);
		txid[TXID_CHARS] = 0;
		fprintf(stderr, "%s: fetching directory at %s\n", clntip, txid);
		FILE *tmpDirFp;
		if ((tmpDirFp = tmpfile()) == NULL) { die("tmpfile() failed"); }
		if (getFile(txid, bitdbNode, fileno(tmpDirFp), NULL)) {
			fprintf(stderr, "%s: directory fetched at %s\n", clntip, txid);
			rewind(tmpDirFp);
			char lineBuf[DIR_LINE_BUF];
			int matched = 0;
			while (fgets(lineBuf, DIR_LINE_BUF, tmpDirFp) != NULL) {
				if (strncmp(lineBuf, realUrl, strlen(realUrl)) == 0) {
					matched = 1;
					fgets(txid, TXID_CHARS+1, tmpDirFp);
					txid[TXID_CHARS] = 0;
					fprintf(stderr, "%s: fetching/serving file at %s\n", clntip, txid);
					getFile(txid, bitdbNode, info_fd->connect_fd, &cashFoundHandler);
					break;
				}
			}
			if (!matched) {
				fprintf(stderr, "%s: requested file absent in directory at %s\n", clntip, txid);
				cashFoundHandler(0, info_fd->connect_fd);
			}
		} else { fprintf(stderr, "%s: nothing found at %s\n", clntip, txid); cashFoundHandler(0, info_fd->connect_fd); }
		fclose(tmpDirFp);
	} else { 
		fprintf(stderr, "%s: fetching/serving file at %s\n", clntip, url+1);
		getFile(url+1, bitdbNode, info_fd->connect_fd, &cashFoundHandler);
	}

	return MHD_NO;
}

int main() {
	bitdbNode = BITDB_DEFAULT;
	struct MHD_Daemon *d;
	if ((d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
				  80,
				  NULL,
				  NULL,
				  &requestHandler,
				  NULL,
				  MHD_OPTION_END)) == NULL) { die("MHD_start_daemon() failed"); }
	(void) getc (stdin);
	MHD_stop_daemon(d);
	return 0;
}
