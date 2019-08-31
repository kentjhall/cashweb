#include <cashgettools.h>
#include <microhttpd.h>
#include <getopt.h>

#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define BITDB_DEFAULT "https://bitdb.bitcoin.com/q"
#define CS_PORT_DEFAULT 80
#define TMP_DIRFILE_PATH_DEFAULT "/tmp/"
#define TMP_DIRFILE_TIMEOUT_DEFAULT 20

typedef int CS_CW_STATUS;
#define CS_REQUEST_HOST_NO -1
#define CS_REQUEST_CWID_NO -2
#define CS_SYS_ERR -3

#define RESP_BUF 110
#define REQ_DESCRIPT_BUF 50
#define TRAILING_BACKSLASH_APPEND "index.html"
#define MIME_STR_DEFAULT "application/octet-stream"
#define TMP_DIRFILE_PREFIX "cashserver-"

#define DOT_COUNT(h,c) for (c=0; h[c]; h[c]=='.' ? c++ : *h++);

static bool dirBySubdomain = true;
static const char *tmpDirfilePath = TMP_DIRFILE_PATH_DEFAULT;
static unsigned int tmpDirfileTimeout = TMP_DIRFILE_TIMEOUT_DEFAULT;
static struct CWG_params genGetParams;

struct cashRequestData {
	const char *cwId;
	const char *name;
	const char *path;
	const char *pathReplace;
	char *resMimeType;
	const char *clntip;
};

static inline void initCashRequestData(struct cashRequestData *requestData, const char *clntip, char *resMimeType) {
	requestData->cwId = NULL;
	requestData->name = NULL;
	requestData->path = NULL;
	requestData->pathReplace = NULL;
	requestData->resMimeType = resMimeType;	
	requestData->clntip = clntip;
}

static char *cashStatusToResponseMsg(CS_CW_STATUS status) {
	switch (status) {
		case CW_OK:
			return "200 OK";
		case CW_DATADIR_NO:
		case CWG_FETCH_ERR:
		case CWG_WRITE_ERR:
		case CW_SYS_ERR:
		case CS_SYS_ERR:
			return "500 Internal Server Error";
		case CS_REQUEST_HOST_NO:
		case CS_REQUEST_CWID_NO:
			return "400 Bad Request";
		default:
			return "404 Not Found";
	}
}

static void cashFoundHandler(CS_CW_STATUS status, void *requestData, int sockfd) {
	struct cashRequestData *rd = (struct cashRequestData *)requestData;
	const char *errMsg;
	if (status == CW_OK) { errMsg = ""; }
	else if (status == CS_REQUEST_HOST_NO) { errMsg = "Request is missing host header."; }
	else if (status == CS_REQUEST_CWID_NO) { errMsg = "Invalid cashserver request format; no identifier specified."; }
	else if (status == CS_SYS_ERR) { errMsg = CWG_errno_to_msg(CW_SYS_ERR); }
	else { errMsg = CWG_errno_to_msg(status); }
	int bufSz = RESP_BUF + strlen(errMsg);

	int reqDBufSz = REQ_DESCRIPT_BUF;
	const char *clntip = rd ? rd->clntip : NULL;
	const char *reqCwId = rd ? rd->cwId : NULL;

	char id[CW_NAMETAG_ID_MAX_LEN+1];
	const char *reqPath = rd ? rd->path : NULL;
	if (reqCwId && !reqPath && CW_is_valid_path_id(reqCwId, id, &reqPath)) { reqCwId = id; }

	const char *reqName = rd ? rd->name : NULL;
	int reqRevision = CW_REV_LATEST;
	if (reqCwId && !reqName) { CW_is_valid_nametag_id(reqCwId, &reqRevision, &reqName); }

	char *mimeType = rd ? rd->resMimeType : NULL;
	if (mimeType && mimeType[0] == 0) { strcat(mimeType, MIME_STR_DEFAULT); }
	if (reqCwId) { reqDBufSz += strlen(reqCwId); }
	if (reqPath) { reqDBufSz += strlen(reqPath); }
	char dummy[2];
	char reqRevisionStr[snprintf(dummy, sizeof(dummy), "%d", reqRevision) + strlen("LATEST") + 1]; reqRevisionStr[0] = 0;
	if (reqName) {
		reqDBufSz += strlen(reqName);
		if (reqRevision >= 0) { snprintf(reqRevisionStr, sizeof(reqRevisionStr), "%d", reqRevision); }
		else { strcat(reqRevisionStr, "LATEST"); }
		reqDBufSz += strlen(reqRevisionStr);
	}
	char reqDescript[reqDBufSz]; reqDescript[0] = 0;
	bufSz += reqDBufSz;
	
	char respStatus[bufSz];
	if (status == CW_OK) {
		if (reqName) {
			if (reqPath) {
				fprintf(stderr, "%s: serving file at name '%s', revision %s, path %s, type '%s'\n",
						 clntip ? clntip : "?", reqName, reqRevisionStr, reqPath, mimeType ? mimeType : "?");
			} else {
				fprintf(stderr, "%s: serving file at name '%s', revision %s, type '%s'\n",
						 clntip ? clntip : "?", reqName, reqRevisionStr, mimeType ? mimeType : "?");
			}
		} else {
			if (reqCwId && reqPath) {
				fprintf(stderr, "%s: serving file at txid %s, path %s, type '%s'\n",
						 clntip ? clntip : "?", reqCwId, reqPath, mimeType ? mimeType : "?");
			} else {
				fprintf(stderr, "%s: serving file at txid %s, type '%s'\n",
						 clntip ? clntip : "?", reqCwId ? reqCwId : "?", mimeType ? mimeType : "?");
			}
		}

		char contentTypeHeader[CWG_MIMESTR_BUF+20]; contentTypeHeader[0] = 0;
		if (mimeType) { snprintf(contentTypeHeader, sizeof(contentTypeHeader), "\r\nContent-Type: %s", mimeType); }

		snprintf(respStatus, sizeof(respStatus), "HTTP/1.1 %s%s\r\n\r\n",
			 cashStatusToResponseMsg(status), contentTypeHeader);
	} else {
		char *httpErrMsg = cashStatusToResponseMsg(status);
		if (reqName) {
			if (reqPath) {
				snprintf(reqDescript, sizeof(reqDescript), " - name: '%s', revision: %s, path: %s", reqName, reqRevisionStr, reqPath);
			}
			else { snprintf(reqDescript, reqDBufSz, " - name: '%s', revision: %s", reqName, reqRevisionStr); }
		} else if (reqCwId) {
			if (reqPath) {
				snprintf(reqDescript, sizeof(reqDescript), " - txid: %s, path: %s", reqCwId, reqPath);
			}
			else { snprintf(reqDescript, reqDBufSz, " - txid: %s", reqCwId); }
		}

		snprintf(respStatus, sizeof(respStatus),
			"HTTP/1.1 %s\r\nContent-Type: text/html\r\n\r\n<html><body><h1>%s</h1><h2>%s%s</h2></body></html>\r\n",
			 httpErrMsg, httpErrMsg, errMsg, reqDescript);
	}
	
	if (send(sockfd, respStatus, strlen(respStatus), 0) != strlen(respStatus)) { perror("send() failed on response"); fprintf(stderr, "HEREnow: %d\n", sockfd); }
}

static CS_CW_STATUS cashGetDirPathId(struct cashRequestData *dirReq, struct CWG_params *params, char **pathId);

static CS_CW_STATUS cashGetDirPathIdFromStream(FILE *dirStream, const char *path, const char *tmpDirfileName, struct CWG_params *params, char **pathId) {
	CW_STATUS status;
	struct cashRequestData *rd = (struct cashRequestData *)params->foundHandleData;
	const char *clntip = rd->clntip;

	fprintf(stderr, "%s: getting path %s from directory index at %s\n", clntip, path, tmpDirfileName);
	char *pathIdN = NULL;
	char *subPath = NULL;

	status = CWG_dirindex_path_to_identifier(dirStream, path, &subPath, &pathIdN);
	if (status == CW_OK) {
		fprintf(stderr, "%s: path %s at directory index %s resolved to be %s; %sremaining path %s\n", clntip, path, tmpDirfileName, pathIdN, subPath ? "" : "no ", subPath ? subPath : "");	
		if (subPath) {
			struct cashRequestData dirReqN;
			initCashRequestData(&dirReqN, clntip, NULL);
			dirReqN.cwId = pathIdN;
			dirReqN.path = subPath;
			status = cashGetDirPathId(&dirReqN, params, pathId);
		}
		else if ((*pathId = strdup(pathIdN)) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; }
	}

	if (subPath) { free(subPath); }
	if (pathIdN) { free(pathIdN); }
	return status;
}

static CS_CW_STATUS cashGetDirPathId(struct cashRequestData *dirReq, struct CWG_params *params, char **pathId) {
	CW_STATUS status;
	const char *clntip = dirReq->clntip;
	const char *path = dirReq->path;
	if (!path) { fprintf(stderr, "ERROR: no path provided to cashGetDirPathId(); problem with cashserver\n"); return CS_SYS_ERR; }

	char nametagId[CW_NAMETAG_ID_MAX_LEN+1];
	const char *dirId;
	if (dirReq->cwId) { dirId = dirReq->cwId; }
	else if (dirReq->name) { CW_construct_nametag_id(dirReq->name, CW_REV_LATEST, &nametagId); dirId = nametagId; }
	else { fprintf(stderr, "ERROR: no identifier provided to cashGetDirPathId(); problem with cashserver\n"); return CS_SYS_ERR; }

	char tmpDirfileName[strlen(tmpDirfilePath) + strlen(TMP_DIRFILE_PREFIX) + strlen(dirId) + 1]; tmpDirfileName[0] = 0;
	strcat(tmpDirfileName, tmpDirfilePath);
	strcat(tmpDirfileName, TMP_DIRFILE_PREFIX);
	strcat(tmpDirfileName, dirId);

	FILE *dirFp;
	if (access(tmpDirfileName, F_OK) != -1 && (dirFp = fopen(tmpDirfileName, "rb"))) {
		status = cashGetDirPathIdFromStream(dirFp, path, tmpDirfileName, params, pathId);
		if (status == CWG_IN_DIR_NO && dirReq->pathReplace) {
			rewind(dirFp);
			status = cashGetDirPathIdFromStream(dirFp, dirReq->pathReplace, tmpDirfileName, params, pathId);
		}
		fclose(dirFp);
		if (status == CWG_IS_DIR_NO) {
			if (unlink(tmpDirfileName) != -1) { fprintf(stderr, "unlinking saved directory index at %s; invalid directory index\n", tmpDirfileName); }
		}
		return status;
	}

	int dirFildes;
	if ((dirFildes = open(tmpDirfileName, O_WRONLY | O_CREAT, 0600)) > -1) {
		fprintf(stderr, "%s: saving requested directory index at identifier '%s' - %s\n", clntip, dirId, tmpDirfileName);
		struct CWG_params paramsD;
		copy_CWG_params(&paramsD, params);
		paramsD.forceDir = true;
		paramsD.foundHandler = NULL;
		const char *identifier;
		if (dirReq->cwId) {
			identifier = dirReq->cwId;
			status = CWG_get_by_id(identifier, &paramsD, dirFildes);
		}
		else if (dirReq->name) {
			identifier = dirReq->name;
			status = CWG_get_by_name(identifier, CW_REV_LATEST, &paramsD, dirFildes);
		}
		else {
			fprintf(stderr, "ERROR: no identifier provided to cashGetDirPathId(); problem with cashserver\n");
			status = CS_SYS_ERR;
		}
		close(dirFildes);
		if (status != CW_OK) {
			fprintf(stderr, "%s: failed to get directory index at identifier '%s'\n", clntip, identifier);
			unlink(tmpDirfileName);
			return status;
		}

		pid_t pid = fork();
		if (pid == 0) {
			sleep(tmpDirfileTimeout);		
			if (unlink(tmpDirfileName) != -1) { fprintf(stderr, "unlinking saved directory index at %s; timeout\n", tmpDirfileName); }
			exit(0);
		} else if (pid < 0) {
			if (unlink(tmpDirfileName) != -1) { fprintf(stderr, "unlinking saved directory index at %s; fork failure\n", tmpDirfileName); }
			perror("fork() failed");
			return CS_SYS_ERR;
		}	

		return cashGetDirPathId(dirReq, params, pathId);
	}

	fprintf(stderr, "ERROR: failed to save/read directory index at %s\n", tmpDirfileName);
	perror("mkstemp() failed");
	return CS_SYS_ERR;
}

static CS_CW_STATUS cashRequestHandleByUri(const char *url, const char *clntip, int sockfd) {
	char mimeType[CWG_MIMESTR_BUF]; mimeType[0] = 0;

	struct cashRequestData rd;
	initCashRequestData(&rd, clntip, mimeType);

	struct CWG_params getParams;
	copy_CWG_params(&getParams, &genGetParams);
	getParams.foundHandleData = &rd;
	getParams.saveMimeStr = &mimeType;

	const char *idQuery = rd.cwId = url+1;
	if (!CW_is_valid_cashweb_id(idQuery)) { cashFoundHandler(CS_REQUEST_CWID_NO, NULL, sockfd); return CS_REQUEST_CWID_NO; } 
	int idQueryLen = strlen(idQuery);

	char reqPathReplace[idQueryLen + strlen(TRAILING_BACKSLASH_APPEND) + 1]; 

	CW_STATUS status = CW_OK;
	char *pathId = NULL;
	char justId[CW_NAMETAG_ID_MAX_LEN];
	const char *pathPtr;
	if (tmpDirfileTimeout > 0 && CW_is_valid_path_id(idQuery, justId, &pathPtr)) {
		reqPathReplace[0] = 0;
		if (idQuery[idQueryLen-1] == '/') {
			strcat(reqPathReplace, pathPtr);
			strcat(reqPathReplace, TRAILING_BACKSLASH_APPEND);
		}

		struct cashRequestData dirRd;
		initCashRequestData(&dirRd, clntip, NULL);
		dirRd.cwId = justId;
		dirRd.path = getParams.dirPath = (char *)pathPtr;
		dirRd.pathReplace = reqPathReplace;

		CS_CW_STATUS tmpdirStatus;
		if ((tmpdirStatus = cashGetDirPathId(&dirRd, &getParams, &pathId)) == CW_OK) { idQuery = pathId; }
		else if (tmpdirStatus != CS_SYS_ERR) { cashFoundHandler(tmpdirStatus, &rd, sockfd); status = tmpdirStatus; goto cleanup; }
	}

	fprintf(stderr, "%s: fetching requested file at identifier '%s'\n", clntip, idQuery);
	getParams.dirPath = NULL;
	status = CWG_get_by_id(idQuery, &getParams, sockfd);

	cleanup:
		if (pathId) { free(pathId); }
		return status;
}

static CS_CW_STATUS cashRequestHandleBySubdomain(const char *host, const char *url, const char *clntip, int sockfd) {
	char mimeType[CWG_MIMESTR_BUF]; mimeType[0] = 0;

	struct cashRequestData rd;
	initCashRequestData(&rd, clntip, mimeType);

	struct CWG_params getParams;
	copy_CWG_params(&getParams, &genGetParams);
	getParams.foundHandleData = &rd;
	getParams.saveMimeStr = &mimeType;

	int endPos = strlen(host);
	int counter = 0;
	for (int i=endPos-1; i>=0; i--) {
		if (host[i] == '.') {
			endPos = i;
			if (++counter > 1) { break; }
		}
	}	

	char reqName[endPos+1]; reqName[0] = 0;
	strncat(reqName, host, endPos);	
	rd.name = reqName;

	int urlLen = strlen(url);
	char reqPath[urlLen + 1]; reqPath[0] = 0;
	char reqPathReplace[sizeof(reqPath) + strlen(TRAILING_BACKSLASH_APPEND)]; reqPathReplace[0] = 0;
	strcat(reqPath, url);
	if (url[urlLen-1] == '/') {
		strcat(reqPathReplace, reqPath);
		strcat(reqPathReplace, TRAILING_BACKSLASH_APPEND);
	}

	rd.path = getParams.dirPath = reqPath;
	rd.pathReplace = reqPathReplace;

	CW_STATUS status = CW_OK;
	char *pathId = NULL;
	CS_CW_STATUS tmpdirStatus = CW_OK;
	if (tmpDirfileTimeout > 0 && (tmpdirStatus = cashGetDirPathId(&rd, &getParams, &pathId)) == CW_OK) {
		fprintf(stderr, "%s: fetching file at identifier '%s'\n", clntip, pathId);
		getParams.dirPath = NULL;
		status = CWG_get_by_id(pathId, &getParams, sockfd);
		goto cleanup;
	} else if (tmpdirStatus != CS_SYS_ERR) { cashFoundHandler(tmpdirStatus, &rd, sockfd); status = tmpdirStatus; goto cleanup; }

	fprintf(stderr, "%s: fetching requested file at name '%s', path %s\n", clntip, rd.name, rd.path);
	status = CWG_get_by_name(rd.name, CW_REV_LATEST, &getParams, sockfd);

	cleanup:
		if (pathId) { free(pathId); }
		return status;
}

static inline CS_CW_STATUS cashRequestHandle(struct MHD_Connection *connection, const char *url, const char *clntip, int sockfd) {
	const char *host = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Host");
	if (host == NULL) { cashFoundHandler(CS_REQUEST_HOST_NO, NULL, sockfd); return CS_REQUEST_HOST_NO; }

	const char *hostPtr = host;
	int dotCount;
	DOT_COUNT(hostPtr, dotCount);	

	if (dirBySubdomain && dotCount > 1) {
		fprintf(stderr, "%s: requested %s%s\n", clntip, host, url);
		return cashRequestHandleBySubdomain(host, url, clntip, sockfd);
	}
	
	fprintf(stderr, "%s: requested %s\n", clntip, url);
	return cashRequestHandleByUri(url, clntip, sockfd);
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
	int sockfd = info_fd->connect_fd;

	CS_CW_STATUS status;

	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) { perror("fcntl() failed on getting flags for socket descriptor"); status = CS_SYS_ERR; goto print; }
	flags &= ~O_NONBLOCK;
	if (fcntl(sockfd, F_SETFL, flags) == -1) { perror("fcntl() failed on setting flags for socket descriptor"); status = CS_SYS_ERR; goto print; }
	
	status = cashRequestHandle(connection, url, clntip, info_fd->connect_fd);

	print:
	if (status == CW_OK) { fprintf(stderr, "%s: requested file fetched and served\n", clntip); }
	else if (status == CS_REQUEST_HOST_NO) { fprintf(stderr, "%s: bad request, no host header\n", clntip); }
	else if (status == CS_REQUEST_CWID_NO) { fprintf(stderr, "%s: bad request %s, invalid identifier\n", clntip, url); }
	else if (status == CS_SYS_ERR) { fprintf(stderr, "%s: cashserver-level system error\n", clntip); }
	else { fprintf(stderr, "%s: request %s resulted in error code %d: %s\n", clntip, url, status, CWG_errno_to_msg(status)); }

	return MHD_NO;
}

int main(int argc, char **argv) {
	unsigned short port = CS_PORT_DEFAULT;
	char *mongodb = NULL;
	char *bitdbNode = BITDB_DEFAULT;
	bool no = false;

	int c;
	while ((c = getopt(argc, argv, ":nht:d:p:m:lb:")) != -1) {
		switch (c) {
			case 'n':
				no = true;
				break;
			case 'h':
				dirBySubdomain = no ? false : true;
				no = false;
				break;
			case 't':
				tmpDirfileTimeout = atoi(optarg);
				break;
			case 'd':
				tmpDirfilePath = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'm':
				mongodb = optarg;
				break;
			case 'l':
				mongodb = MONGODB_LOCAL_ADDR;
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
				fprintf(stderr, "getopt() unknown error\n");
				exit(1);
		}
	}	

	init_CWG_params(&genGetParams, NULL, bitdbNode, NULL);
	genGetParams.foundHandler = &cashFoundHandler;

	if (mongodb) { CWG_init_mongo_pool(mongodb, &genGetParams); }
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
	if (mongodb) { CWG_cleanup_mongo_pool(&genGetParams); } 

	return 0;
}
