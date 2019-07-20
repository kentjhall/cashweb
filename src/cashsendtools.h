#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include "cashwebuni.h"
#include <fts.h>
#include <libbitcoinrpc/bitcoinrpc.h>

/* cashsendtools error codes */
#define CWS_RPC_NO 3
#define CWS_CONFIRMS_NO 4
#define CWS_FEE_NO 5
#define CWS_FUNDS_NO 6
#define CWS_RPC_ERR 7

/*
 * params for sending
 * rpcServer: RPC address
 * rpcPort: RPC port
 * rpcUser: RPC username
 * rpcPass: RPC password 
 * maxTreeDepth: maximum depth for file tree (will send as chained tree if hit);
 		 determines the chunk size the file is downloaded in
 * cwType: the file's cashweb type (set to CW_T_MIMESET if the mimetype should be interpreted by extension)
 * datadir: specify data directory path for cashwebtools;
 	    can be left as NULL if cashwebtools is properly installed on system with 'make install'
 */
struct CWS_params {
	const char *rpcServer;
	unsigned short rpcPort;
	const char *rpcUser;
	const char *rpcPass;
	int maxTreeDepth;
	CW_TYPE cwType;
	const char *datadir;
};

/*
 * initializes struct CWS_params
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
	csp->datadir = NULL;
}

/*
 * copies struct CWS_params from source to dest
 */
inline void copy_CWS_params(struct CWS_params *dest, struct CWS_params *source) {
	dest->rpcServer = source->rpcServer;
	dest->rpcPort = source->rpcPort;
	dest->rpcUser = source->rpcUser;
	dest->rpcPass = source->rpcPass;
	dest->maxTreeDepth = source->maxTreeDepth;
	dest->cwType = source->cwType;
	dest->datadir = source->datadir;
}

/*
 * sends file from stream as per options set in params
 * cashsendtools does not ascertain mimetype when sending from stream, so if type is set to CW_T_MIMESET in params,
   will default to CW_T_FILE
 * cost of the send is written to balanceDiff if set; if not needed, can be set to NULL
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
 * directory index will always be sent with type CW_T_DIR, regardless of params
 */
CW_STATUS CWS_send_dir_from_path(const char *path, struct CWS_params *params,  double *balanceDiff, char *resTxid);

/*
 * determines protocol-specific cashweb type value for given filename/extension by mimetype and copies to given struct CWS_params
 * this function is public in case the user wants to force a mimetype other than what matches the file extension,
   or more likely, if a mimetype needs to be set when sending from stream
 * uses datadir path stored in params to find cashweb protocol-specific mime.types;
   if this is NULL, will set to proper cashwebtools system install data directory
 */
CW_STATUS CWS_set_cw_mime_type_by_extension(const char *fname, struct CWS_params *csp);

/*
 * returns generic error message by error code
 */
const char *CWS_errno_to_msg(int errNo);

#endif
