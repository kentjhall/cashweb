#include "cashgettools.h"
#include <microhttpd.h>

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"
#define CS_PORT_DEFAULT 80

typedef int CS_CW_STATUS;
#define CS_REQUEST_NO -1

#define RESP_BUF 110
#define REQ_DESCRIPT_BUF 25
#define MIMETYPE_BUF 256
#define TRAILING_BACKSLASH_APPEND "index.html"

static uint16_t port = CS_PORT_DEFAULT;
static char *mongodb = NULL;
static char *bitdbNode = BITDB_DEFAULT;
static bool dirBySubdomain = false;

struct cashRequestData {
	const char *cwId;
	const char *path;
	const char *clntip;
	char *resMimeType;
};

static inline void initCashRequestData(struct cashRequestData *requestData, const char *clntip, char *resMimeType) {
	requestData->cwId = NULL;
	requestData->path = NULL;
	requestData->clntip = clntip;
	requestData->resMimeType = resMimeType;	
}

static char *cashStatusToResponseMsg(CS_CW_STATUS status) {
	switch (status) {
		case CW_OK:
			return "200 OK";
		case CW_DATADIR_NO:
		case CWG_FETCH_ERR:
		case CWG_WRITE_ERR:
		case CW_SYS_ERR:
			return "500 Internal Server Error";
		case CS_REQUEST_NO:
			return "400 Bad Request";
		default:
			return "404 Not Found";
	}
}

static void cashFoundHandler(CS_CW_STATUS status, void *requestData, int sockfd) {
	struct cashRequestData *rd = (struct cashRequestData *)requestData;
	const char *errMsg;
	if (status == CW_OK) { errMsg = ""; }
	else if (status == CS_REQUEST_NO) { errMsg = "Request is missing host header."; }
	else { errMsg = CWG_errno_to_msg(status); }
	int bufSz = RESP_BUF + strlen(errMsg);

	int reqDBufSz = REQ_DESCRIPT_BUF;
	const char *clntip = rd && rd->clntip ? rd->clntip : NULL;
	const char *reqCwId = rd && rd->cwId ? rd->cwId : NULL;
	const char *reqPath = rd && rd->path ? rd->path : NULL;
	const char *mimeType = rd && rd->resMimeType ? rd->resMimeType : NULL;
	if (reqCwId) { reqDBufSz += strlen(reqCwId); }
	if (reqPath) { reqDBufSz += strlen(reqPath); }
	char reqDescript[reqDBufSz]; reqDescript[0] = 0;
	bufSz += reqDBufSz;
	
	char respStatus[bufSz];
	if (status == CW_OK) {
		if (reqCwId && reqPath) {
			fprintf(stderr, "%s: serving file at identifier %s, path %s: type '%s'\n",
					 clntip ? clntip : "?", reqCwId, reqPath, mimeType ? mimeType : "unknown");
		} else {
			fprintf(stderr, "%s: serving file at identifier %s: type '%s'\n",
					 clntip ? clntip : "?", reqCwId ? reqCwId : "?", mimeType ? mimeType : "unknown");
		}

		char contentTypeHeader[MIMETYPE_BUF+20]; contentTypeHeader[0] = 0;
		if (mimeType) { snprintf(contentTypeHeader, sizeof(contentTypeHeader), "\r\nContent-Type: %s", mimeType); }

		snprintf(respStatus, sizeof(respStatus), "HTTP/1.1 %s%s\r\n\r\n",
			 cashStatusToResponseMsg(status), contentTypeHeader);
	} else {
		char *httpErrMsg = cashStatusToResponseMsg(status);
		if (reqCwId && reqPath) {
			snprintf(reqDescript, reqDBufSz, " - identifier: %s, path: %s", reqCwId, reqPath);
		}
		else if (reqCwId) { snprintf(reqDescript, reqDBufSz, " - identifier: %s", reqCwId); }

		snprintf(respStatus, bufSz,
			"HTTP/1.1 %s\r\nContent-Type: text/html\r\n\r\n<html><body><h1>%s</h1><h2>%s%s</h2></body></html>\r\n",
			 httpErrMsg, httpErrMsg, errMsg, reqDescript);
	}

	if (send(sockfd, respStatus, strlen(respStatus), 0) != strlen(respStatus)) { perror("send() failed on response"); }
}

static CW_STATUS cashRequestHandleByUri(const char *url, const char *clntip, int sockfd) {
	char mimeTypePtr[MIMETYPE_BUF]; mimeTypePtr[0] = 0;

	struct cashRequestData rd;
	initCashRequestData(&rd, clntip, mimeTypePtr);


	struct CWG_params getParams;
	init_CWG_params(&getParams, mongodb, bitdbNode);
	getParams.foundHandler = &cashFoundHandler;	
	getParams.foundHandleData = &rd;
	getParams.saveMimeStr = mimeTypePtr;

	int urlLen = strlen(url);

	char reqCwId[urlLen+1]; reqCwId[0] = 0;
	char reqPath[urlLen + strlen(TRAILING_BACKSLASH_APPEND) + 1]; reqPath[0] = 0;

	const char *urlPtr;
	if ((urlPtr = strstr(url+1, "/")) != NULL) {
		strcat(reqPath, urlPtr);
		if (url[strlen(url)-1] == '/') { strcat(reqPath, TRAILING_BACKSLASH_APPEND); }
		rd.path = getParams.dirPath = reqPath;

		int cwIdLen = urlPtr-(url+1);
		if (cwIdLen > 0) {
			strncat(reqCwId, url+1, cwIdLen);
			rd.cwId = reqCwId;
		}
	} else if (urlLen > 1) { rd.cwId = url+1; }
	
	if (!rd.cwId) { cashFoundHandler(CWG_FETCH_NO, NULL, sockfd); return CWG_FETCH_NO; }

	if (rd.path) {
		fprintf(stderr, "%s: fetching requested file at identifier %s, path %s\n", clntip, rd.cwId, rd.path);
	} else {
		fprintf(stderr, "%s: fetching requested file at identifier %s\n", clntip, rd.cwId);
	}

	return CWG_get_by_txid(rd.cwId, &getParams, sockfd);
}

static CS_CW_STATUS cashRequestHandleBySubdomain(struct MHD_Connection *connection, const char *url, const char *clntip, int sockfd) {
	const char *host = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Host");
	if (host == NULL) { cashFoundHandler(CS_REQUEST_NO, NULL, sockfd); return CS_REQUEST_NO; }

	char mimeTypePtr[MIMETYPE_BUF]; mimeTypePtr[0] = 0;

	struct cashRequestData rd;
	initCashRequestData(&rd, clntip, mimeTypePtr);

	struct CWG_params getParams;
	init_CWG_params(&getParams, mongodb, bitdbNode);
	getParams.foundHandler = &cashFoundHandler;	
	getParams.foundHandleData = &rd;
	getParams.saveMimeStr = mimeTypePtr;

	int endPos = strlen(host);
	int counter = 0;
	for (int i=endPos-1; i>=0; i--) {
		if (host[i] == '.') {
			endPos = i;
			if (++counter > 1) { break; }
		}
	}

	int urlLen = strlen(url);
	
	char reqCwId[endPos+1]; reqCwId[0] = 0;
	strncat(reqCwId, host, endPos);	
	if (endPos > 0) { rd.cwId = reqCwId; }

	char reqPath[urlLen + strlen(TRAILING_BACKSLASH_APPEND) + 1]; reqPath[0] = 0;
	strcat(reqPath, url);
	if (url[urlLen-1] == '/') { strcat(reqPath, TRAILING_BACKSLASH_APPEND); }
	getParams.dirPath = reqPath;
	if (strcmp(url, "/") == 0) { getParams.foundSuppressErr = CWG_IS_DIR_NO; }
	else { rd.path = getParams.dirPath; }

	if (rd.cwId == NULL) { cashFoundHandler(CWG_FETCH_NO, NULL, sockfd); return CWG_FETCH_NO; }

	if (rd.path) {
		fprintf(stderr, "%s: fetching requested file at identifier %s, path %s\n", clntip, rd.cwId, rd.path);
	} else {
		fprintf(stderr, "%s: fetching requested file at identifier %s\n", clntip, rd.cwId);
	}

	return CWG_get_by_txid(rd.cwId, &getParams, sockfd);
}

static inline CS_CW_STATUS cashRequestHandle(struct MHD_Connection *connection, const char *url, const char *clntip, int sockfd) {
	return dirBySubdomain ? cashRequestHandleBySubdomain(connection, url, clntip, sockfd)
			      : cashRequestHandleByUri(url, clntip, sockfd);	
	
}

static int requestHandler(void *cls,
			  struct MHD_Connection *connection,
			  const char *url,
			  const char *method,
			  const char *version,
			  const char *upload_data,
			  size_t *upload_data_size,
			  void **ptr) {
	if (strcmp(method, "GET") != 0) { return MHD_NO;  }
	if (*upload_data_size != 0) { return MHD_NO; }

	const union MHD_ConnectionInfo *info_fd = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CONNECTION_FD);
	const union MHD_ConnectionInfo *info_addr = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	const char *clntip = inet_ntoa(((struct sockaddr_in *) info_addr->client_addr)->sin_addr);
	fprintf(stderr, "%s: requested %s\n", clntip, url);
	
	CS_CW_STATUS status = cashRequestHandle(connection, url, clntip, info_fd->connect_fd);

	if (status == CW_OK) { fprintf(stderr, "%s: requested file fetched and served\n", clntip); }
	else if (status == CS_REQUEST_NO) { fprintf(stderr, "%s: bad request, no host header\n", clntip); }
	else { fprintf(stderr, "%s: request %s resulted in error code %d: %s\n", clntip, url, status, CWG_errno_to_msg(status)); }

	return MHD_NO;
}

int main(int argc, char **argv) {
	for (int i=1; i<argc; i++) {
		if (strncmp("--port=", argv[i], 7) == 0) { port = atoi(argv[i]+7); }
		if (strncmp("--mongodb=", argv[i], 10) == 0) { mongodb = argv[i]+10; }
		if (strncmp("--mongodb-local", argv[i], 15) == 0) { mongodb = MONGODB_LOCAL_ADDR; }
		if (strncmp("--bitdb=", argv[i], 8) == 0) { bitdbNode = argv[i]+8; }
	}

	struct MHD_Daemon *d;
	if ((d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
				  port,
				  NULL,
				  NULL,
				  &requestHandler,
				  NULL,
				  MHD_OPTION_END)) == NULL) { perror("MHD_start_daemon() failed"); exit(1); }
	fprintf(stderr, "Starting cashserver on port %u... (source is %s at %s)\n", port, mongodb ? "MongoDB" : "BitDB HTTP endpoint", mongodb ? mongodb : bitdbNode);
	(void) getc (stdin);
	MHD_stop_daemon(d);
	return 0;
}
