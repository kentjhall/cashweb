#include "cashgettools.h"
#include <arpa/inet.h>
#include <microhttpd.h>

#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"

#define RESP_BUF 80

static char *bitdbNode;

static void cashFoundHandler(int status, int sockfd) {
	char *errMsg = status != CW_OK ? errNoToMsg(status) : "";
	int buf = RESP_BUF + strlen(errMsg);
	char respStatus[buf];
	snprintf(respStatus, buf, status == CW_OK ? "HTTP/1.1 200 OK\r\n\r\n" :
		 "HTTP/1.1 404 Not Found\r\n\r\n<html><body><h1>Error 404</h1><h2>%s</h2></body></html>\r\n", errMsg);
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

	int status;
	char *realUrl;
	if ((realUrl = strstr(url+1, "/")) != NULL) {
		if (strcmp(realUrl, "/") == 0) { realUrl = "/index.html"; }

		char txid[TXID_CHARS+1];
		strncpy(txid, url+1, TXID_CHARS);
		txid[TXID_CHARS] = 0;

		fprintf(stderr, "%s: fetching directory at %s for file at path %s\n", clntip, txid, realUrl);
		status = getDirFile(txid, realUrl, bitdbNode, NULL, &cashFoundHandler, info_fd->connect_fd);
	} else { 
		fprintf(stderr, "%s: fetching and serving file at %s\n", clntip, url+1);
		status = getFile(url+1, bitdbNode, &cashFoundHandler, info_fd->connect_fd);
	}

	if (status == CW_OK) { fprintf(stderr, "%s: requested file fetched and served\n", clntip); }
	else { fprintf(stderr, "%s: request %s resulted in error code %d: %s\n", clntip, url, status, errNoToMsg(status)); }

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
