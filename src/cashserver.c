#include <cashgettools.h>
#include <microhttpd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <getopt.h>

#define USAGE_STR "usage: %s [FLAGS]\n"
#define HELP_STR \
	USAGE_STR\
	"\n"\
	" Flag    | Use\n"\
	"---------|----------------------------------------------------------------------------------------------------------------------------------------------\n"\
	"[none]   | host CashServer getting by local MongoDB ("MONGODB_LOCAL_ADDR") on port "CS_PORT_DEFAULT"\n"\
	"-p <ARG> | specify hosting port (default if "CS_PORT_DEFAULT")\n"\
	"-m <ARG> | specify MongoDB URI for querying (default is "MONGODB_LOCAL_ADDR")\n"\
	"-b <ARG> | specify BitDB HTTP endpoint URL for querying instead of MongoDB\n"\
	"-d <ARG> | specify location of valid cashwebtools data directory (default is install directory)\n"\
	"-q <ARG> | specify URI prefix to be recognized for making query (default is "URI_QUERY_PREFIX_DEFAULT")\n"\
	"-ns      | disable default behavior to treat any subdomain (*.X.X) in HTTP host header as a named CashWeb directory request\n"\
	"-f <ARG> | specify path for temporarily stored directory indexes (default is "TMP_DIRFILE_PATH_DEFAULT")\n"\
	"-t <ARG> | specify timeout for a temporarily stored directory index to be destroyed (default is "TMP_DIRFILE_TIMEOUT_DEFAULT"s); set 0 for disabling temporary storage\n"


#define MONGODB_LOCAL_ADDR "mongodb://localhost:27017"
#define CS_PORT_DEFAULT "80"
#define URI_QUERY_PREFIX_DEFAULT "/q"
#define DIR_BY_SUBDOMAIN_DEFAULT true
#define TMP_DIRFILE_PATH_DEFAULT "/tmp/"
#define TMP_DIRFILE_TIMEOUT_DEFAULT "20"

typedef char CS_CW_STATUS;
#define CS_REQUEST_HOST_NO -1
#define CS_REQUEST_CWID_NO -2
#define CS_SYS_ERR -3

#define RESPONSE_CALLBACK_BLOCK_SZ 1024
#define RESP_BUF 110
#define REQ_DESCRIPT_BUF 50
#define TRAILING_BACKSLASH_APPEND "index.html"
#define MIME_STR_DEFAULT "application/octet-stream"
#define TMP_DIRFILE_PREFIX "cashserver-"

#define DOT_COUNT(h,c) for (c=0; h[c]; h[c]=='.' ? c++ : *h++);

static struct CWG_params genGetParams;
static const char *defaultGetId;
static const char *uriQueryPrefix;
static bool dirBySubdomain;
static const char *tmpDirfilePath;
static unsigned int tmpDirfileTimeout;

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

static int cashStatusToResponseCode(CS_CW_STATUS status) {
	switch (status) {
		case CW_OK:
			return MHD_HTTP_OK;
		case CW_DATADIR_NO:
		case CWG_FETCH_ERR:
		case CWG_WRITE_ERR:
		case CW_SYS_ERR:
		case CS_SYS_ERR:
			return MHD_HTTP_INTERNAL_SERVER_ERROR;
		case CS_REQUEST_HOST_NO:
		case CS_REQUEST_CWID_NO:
			return MHD_HTTP_BAD_REQUEST;
		default:
			return MHD_HTTP_NOT_FOUND;
	}
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

static void cashFoundHandler(CS_CW_STATUS status, void *requestData, int respfd) {
	struct cashRequestData *rd = (struct cashRequestData *)requestData;	

	const char *mimeType = rd && rd->resMimeType && rd->resMimeType[0] ? rd->resMimeType : MIME_STR_DEFAULT;
	if (status != CW_OK) { mimeType = "text/html"; }

	if (write(respfd, &status, 1) < 1) { perror("write() failed on respfd status"); }
	if (write(respfd, mimeType, CWG_MIMESTR_BUF) < CWG_MIMESTR_BUF) { perror("write() failed on resfd mimetype"); }

	const char *errMsg = "";
	if (status == CS_REQUEST_HOST_NO) { errMsg = "Request is missing host header."; }
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

	if (status != CW_OK) {
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
			"<html><body><h1>%s</h1><h2>%s%s</h2></body></html>",
			 httpErrMsg, errMsg, reqDescript);

		
		size_t respStatusLen = strlen(respStatus);
		if (write(respfd, respStatus, respStatusLen) < respStatusLen) { perror("write() failed on response body"); }
	} else {
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
	}
}

static CS_CW_STATUS cashGetDirPathId(struct cashRequestData *dirReq, struct CWG_params *params, int respfd, char **pathId);

static CS_CW_STATUS cashGetDirPathIdFromStream(FILE *dirStream, const char *path, const char *tmpDirfileName, struct CWG_params *params, int respfd, char **pathId) {
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
			status = cashGetDirPathId(&dirReqN, params, respfd, pathId);
		}
		else if ((*pathId = strdup(pathIdN)) == NULL) { perror("strdup() failed"); status = CW_SYS_ERR; }
	}

	if (subPath) { free(subPath); }
	if (pathIdN) { free(pathIdN); }
	return status;
}

static CS_CW_STATUS cashGetDirPathId(struct cashRequestData *dirReq, struct CWG_params *params, int respfd, char **pathId) {
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
		status = cashGetDirPathIdFromStream(dirFp, path, tmpDirfileName, params, respfd, pathId);
		if (status == CWG_IN_DIR_NO && dirReq->pathReplace) {
			rewind(dirFp);
			status = cashGetDirPathIdFromStream(dirFp, dirReq->pathReplace, tmpDirfileName, params, respfd, pathId);
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
			close(respfd);
			sleep(tmpDirfileTimeout);		
			if (unlink(tmpDirfileName) != -1) { fprintf(stderr, "unlinking saved directory index at %s; timeout\n", tmpDirfileName); }
			exit(0);
		} else if (pid < 0) {
			if (unlink(tmpDirfileName) != -1) { fprintf(stderr, "unlinking saved directory index at %s; fork failure\n", tmpDirfileName); }
			perror("fork() failed");
			return CS_SYS_ERR;
		}	

		return cashGetDirPathId(dirReq, params, respfd, pathId);
	}

	fprintf(stderr, "ERROR: failed to save/read directory index at %s\n", tmpDirfileName);
	perror("mkstemp() failed");
	return CS_SYS_ERR;
}

static CS_CW_STATUS cashRequestHandleByUri(const char *url, const char *clntip, int respfd) {
	char mimeType[CWG_MIMESTR_BUF]; memset(mimeType, 0, CWG_MIMESTR_BUF);

	struct cashRequestData rd;
	initCashRequestData(&rd, clntip, mimeType);

	struct CWG_params getParams;
	copy_CWG_params(&getParams, &genGetParams);
	getParams.foundHandleData = &rd;
	getParams.saveMimeStr = &mimeType;

	const char *idQuery = rd.cwId = url+1;
	if (!CW_is_valid_cashweb_id(idQuery)) { cashFoundHandler(CS_REQUEST_CWID_NO, NULL, respfd); return CS_REQUEST_CWID_NO; } 
	int idQueryLen = strlen(idQuery);

	char reqPathReplace[idQueryLen + strlen(TRAILING_BACKSLASH_APPEND) + 1]; 

	CW_STATUS status = CW_OK;
	char *pathId = NULL;
	char justId[CW_NAMETAG_ID_MAX_LEN];
	const char *pathPtr;
	if (tmpDirfileTimeout > 0 && CW_is_valid_path_id(idQuery, justId, &pathPtr)) {
		struct cashRequestData dirRd;
		initCashRequestData(&dirRd, clntip, NULL);
		dirRd.cwId = justId;
		dirRd.path = getParams.dirPath = (char *)pathPtr;

		reqPathReplace[0] = 0;
		if (idQuery[idQueryLen-1] == '/') {
			strcat(reqPathReplace, pathPtr);
			strcat(reqPathReplace, TRAILING_BACKSLASH_APPEND);
			dirRd.pathReplace = reqPathReplace;
		}
	
		CS_CW_STATUS tmpdirStatus;
		if ((tmpdirStatus = cashGetDirPathId(&dirRd, &getParams, respfd, &pathId)) == CW_OK) { idQuery = pathId; }
		else if (tmpdirStatus != CS_SYS_ERR) { cashFoundHandler(tmpdirStatus, &rd, respfd); status = tmpdirStatus; goto cleanup; }
	}

	fprintf(stderr, "%s: fetching requested file at identifier '%s'\n", clntip, idQuery);
	getParams.dirPath = NULL;
	status = CWG_get_by_id(idQuery, &getParams, respfd);

	cleanup:
		if (pathId) { free(pathId); }
		return status;
}

static CS_CW_STATUS cashRequestHandleBySubdomain(const char *host, const char *url, const char *clntip, int respfd) {
	char mimeType[CWG_MIMESTR_BUF]; memset(mimeType, 0, CWG_MIMESTR_BUF);

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
	if (tmpDirfileTimeout > 0 && (tmpdirStatus = cashGetDirPathId(&rd, &getParams, respfd, &pathId)) == CW_OK) {
		fprintf(stderr, "%s: fetching file at identifier '%s'\n", clntip, pathId);
		getParams.dirPath = NULL;
		status = CWG_get_by_id(pathId, &getParams, respfd);
		goto cleanup;
	} else if (tmpdirStatus != CS_SYS_ERR) { cashFoundHandler(tmpdirStatus, &rd, respfd); status = tmpdirStatus; goto cleanup; }

	fprintf(stderr, "%s: fetching requested file at name '%s', path %s\n", clntip, rd.name, rd.path);
	status = CWG_get_by_name(rd.name, CW_REV_LATEST, &getParams, respfd);

	cleanup:
		if (pathId) { free(pathId); }
		return status;
}

static inline CS_CW_STATUS cashRequestHandle(struct MHD_Connection *connection, const char *url, const char *clntip, int respfd) {
	const char *host = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Host");
	if (host == NULL) { cashFoundHandler(CS_REQUEST_HOST_NO, NULL, respfd); return CS_REQUEST_HOST_NO; }

	const char *hostPtr = host;
	int dotCount;
	DOT_COUNT(hostPtr, dotCount);	
	size_t uriQueryPrefixLen = strlen(uriQueryPrefix);

	if (dirBySubdomain && dotCount > 1) {
		fprintf(stderr, "%s: requested %s%s\n", clntip, host, url);
		return cashRequestHandleBySubdomain(host, url, clntip, respfd);
	} else if (strncmp(url, uriQueryPrefix, uriQueryPrefixLen) == 0) {
		fprintf(stderr, "%s: queried %s\n", clntip, url+uriQueryPrefixLen);
		return cashRequestHandleByUri(url+uriQueryPrefixLen, clntip, respfd);
	} else if (defaultGetId) {
		char query[1 + strlen(defaultGetId) + strlen(url) + 1]; query[0] = '/'; query[1] = 0;
		strcat(query, defaultGetId);
		strcat(query, url);
		fprintf(stderr, "%s: home request %s\n", clntip, url);
		return cashRequestHandleByUri(query, clntip, respfd);
	} else {
		cashFoundHandler(CS_REQUEST_CWID_NO, NULL, respfd);
		return CS_REQUEST_CWID_NO;
	}
}

static inline ssize_t readPipe(void *cls, uint64_t pos, char *buf, size_t max) {
	int readfd = *(int *)cls;
	ssize_t r = read(readfd, buf, max);

	if (r == 0) { return MHD_CONTENT_READER_END_OF_STREAM; }
	else if (r < 0) { perror("read() failed"); return MHD_CONTENT_READER_END_WITH_ERROR; }
	return r;
}

static inline void closePipeFreeMem(void *cls) {
	close(*(int *)cls);
	free(cls);
}

static int requestHandler(void *cls,
			  struct MHD_Connection *connection,
			  const char *url,
			  const char *method,
			  const char *version,
			  const char *upload_data,
			  size_t *upload_data_size,
			  void **ptr) {
	if (strcmp(method, "GET") != 0) { return MHD_NO; }
	if (*upload_data_size != 0) { return MHD_NO; }

	static int dummy;
	if (*ptr != &dummy) { *ptr = &dummy; return MHD_YES; } 
	*ptr = NULL;

	const union MHD_ConnectionInfo *info_addr = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
	const char *clntip = inet_ntoa(((struct sockaddr_in *) info_addr->client_addr)->sin_addr);

	int pipefd[2];
	if (pipe(pipefd) == -1) { perror("pipe() failed"); return MHD_NO; }

	pid_t pid = fork();
	if (pid == 0) {
		close(pipefd[0]);
		CS_CW_STATUS status = cashRequestHandle(connection, url, clntip, pipefd[1]);	
		if (status == CW_OK) { fprintf(stderr, "%s: requested file fetched and written to response\n", clntip); }
		else if (status == CS_REQUEST_HOST_NO) { fprintf(stderr, "%s: bad request, no host header\n", clntip); }
		else if (status == CS_REQUEST_CWID_NO) { fprintf(stderr, "%s: bad request %s, invalid identifier\n", clntip, url); }
		else if (status == CS_SYS_ERR) { fprintf(stderr, "%s: cashserver-level system error\n", clntip); }
		else { fprintf(stderr, "%s: request %s resulted in error code %d: %s\n", clntip, url, status, CWG_errno_to_msg(status)); }
		close(pipefd[1]);
		exit(0);
	} else if (pid < 0) {
		perror("fork() failed");
		close(pipefd[0]);
		close(pipefd[1]);
		return MHD_NO;
	}
	close(pipefd[1]);

	int *fdstore = malloc(sizeof(int));
	if (!fdstore) { perror("malloc failed"); return MHD_NO; }
	*fdstore = pipefd[0];
	struct MHD_Response *resp = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, RESPONSE_CALLBACK_BLOCK_SZ, &readPipe, fdstore, &closePipeFreeMem);

	CW_STATUS foundStatus;
	char mimeType[CWG_MIMESTR_BUF];
	if (read(pipefd[0], &foundStatus, 1) < 1 || read(pipefd[0], mimeType, CWG_MIMESTR_BUF) < CWG_MIMESTR_BUF) {
		perror("read() failed on respfd");
		return MHD_NO;
	}

	MHD_add_response_header(resp, "Content-Type", mimeType);
	int ret = MHD_queue_response(connection, cashStatusToResponseCode(foundStatus), resp);
	MHD_destroy_response (resp);	

	return ret;
}

int main(int argc, char **argv) {
	init_CWG_params(&genGetParams, NULL, NULL, NULL);
	genGetParams.foundHandler = &cashFoundHandler;

	defaultGetId = NULL;
	uriQueryPrefix = URI_QUERY_PREFIX_DEFAULT;
	dirBySubdomain = DIR_BY_SUBDOMAIN_DEFAULT;;
	tmpDirfilePath = TMP_DIRFILE_PATH_DEFAULT;
	tmpDirfileTimeout = atoi(TMP_DIRFILE_TIMEOUT_DEFAULT);

	unsigned short port = atoi(CS_PORT_DEFAULT);
	char *mongodb = MONGODB_LOCAL_ADDR;

	bool no = false;
	int c;
	while ((c = getopt(argc, argv, ":hp:m:b:d:c:q:nsf:t:")) != -1) {
		switch (c) {
			case 'h':
				fprintf(stderr, HELP_STR, argv[0]);
				exit(0);
			case 'p':
				port = atoi(optarg);
				break;
			case 'm':
				mongodb = optarg;
				break;
			case 'b':
				genGetParams.bitdbNode = optarg;
				mongodb = NULL;
				break;
			case 'd':
				genGetParams.datadir = optarg;
				break;
			case 'c':
				defaultGetId = optarg;
				break;
			case 'q':
				uriQueryPrefix = optarg;
				break;
			case 'n':
				no = true;
				break;
			case 's':
				dirBySubdomain = no ? false : true;
				no = false;
				break;	
			case 'f':
				tmpDirfilePath = optarg;
				break;	
			case 't':
				tmpDirfileTimeout = atoi(optarg);
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

	if (mongodb) { CWG_init_mongo_pool(mongodb, &genGetParams); }
	struct MHD_Daemon *d;
	if ((d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
				  port,
				  NULL,
				  NULL,
				  &requestHandler,
				  NULL,
				  MHD_OPTION_END)) == NULL) { perror("MHD_start_daemon() failed"); exit(1); }
	fprintf(stderr, "Starting cashserver on port %u with home identifier %s... (source is %s at %s)\n", port, defaultGetId ? defaultGetId : "<none>", mongodb ? "MongoDB" : "BitDB HTTP endpoint", mongodb ? mongodb : genGetParams.bitdbNode);

	(void) getc (stdin);
	fprintf(stderr, "Stopping cashserver...\n");
	MHD_stop_daemon(d);
	if (mongodb) { CWG_cleanup_mongo_pool(&genGetParams); } 

	return 0;
}
