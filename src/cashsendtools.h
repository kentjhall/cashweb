#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include "cashwebuni.h"
#include <fts.h>
#include <libbitcoinrpc/bitcoinrpc.h>

/* cashsendtools error codes */
#define CWS_RPC_NO 2
#define CWS_CONFIRMS_NO 3
#define CWS_FEE_NO 4
#define CWS_FUNDS_NO 5
#define CWS_RPC_ERR 6

/*
 * params for sending
 * rpcServer: RPC address
 * rpcPort: RPC port
 * rpcUser: RPC username
 * rpcPass: RPC password
 * maxTreeDepth: maximum depth for file tree (will send as chained tree if hit);
 		 determines the chunk size the file is downloaded in
 * cwType: the file's cashweb type (set to CW_T_MIMESET if the mimetype should be interpreted by extension)
 */
struct CWS_params {
	const char *rpcServer;
	unsigned short rpcPort;
	const char *rpcUser;
	const char *rpcPass;
	int maxTreeDepth;
	CW_TYPE cwType;
};

/*
 * initialize struct CWS_params
 * rpcServer, rpcPort, rpcUser, and rpcPass are required on init
 */
inline void init_CWS_params(struct CWS_params *csp,
				   const char *rpcServer, unsigned short rpcPort, const char *rpcUser, const char *rpcPass) {
	csp->rpcServer = rpcServer;
	csp->rpcPort = rpcPort;
	csp->rpcUser = rpcUser;
	csp->rpcPass = rpcPass;
	csp->maxTreeDepth = -1;
	csp->cwType = CW_T_FILE;
}

/*
 * sends file from stream as per options set in params
 * cost of the send is written to balanceDiff if set; if not needed, set to NULL
 * resultant txid is written to resTxid
 */
CW_STATUS CWS_send_from_stream(FILE *stream, struct CWS_params *params, double *balanceDiff, char *resTxid);

/*
 * sends file at specified path as per options set in params
 * cost of the send us written to balanceDiff if set; if not needed, set to NULL
 * resultant txid is written to resTxid
 */
CW_STATUS CWS_send_from_path(const char *path, struct CWS_params *params, double *balanceDiff, char *resTxid);

/*
 * sends directory at specified path as per options set in params
 * recursively sends all files in directory; then sends directory index
 * all files are sent with given params; this includes maxTreeDepth and cwType
 * set cwType to CW_T_MIMESET for all file mimetypes to be interpreted by extension
 */
CW_STATUS CWS_send_dir_from_path(const char *path, struct CWS_params *params,  double *balanceDiff, char *resTxid);

/*
 * determines protocol-specific cashweb type value for given filename/extension by mimetype and writes to cwType
 * dataDirPath specifies where to find data directory containing mime.types; must follow protocol-specific naming scheme,
   and be contained within CW_mimetypes subdirectory
 * dataDirPath may be set to NULL if cashwebtools is installed on system; will access installed data directory
 */
CW_STATUS CWS_determine_cw_mime_type_by_extension(const char *fname, const char *dataDirPath, CW_TYPE *cwType);

/*
 * returns generic error message by error code
 */
const char *CWS_errno_to_msg(int errNo);

#endif
