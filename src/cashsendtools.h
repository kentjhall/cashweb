#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include "cashwebuni.h"
#include <fts.h>
#include <libbitcoinrpc/bitcoinrpc.h>

#define CWS_RPC_NO 2
#define CWS_CONFIRMS_NO 3
#define CWS_FEE_NO 4
#define CWS_FUNDS_NO 5
#define CWS_RPC_ERR 6

struct CWS_params {
	const char *rpcServer;
	unsigned short rpcPort;
	const char *rpcUser;
	const char *rpcPass;
	int maxTreeDepth;
	CW_TYPE cwType;
};

inline void init_CWS_params(struct CWS_params *csp,
				   const char *rpcServer, unsigned short rpcPort, const char *rpcUser, const char *rpcPass) {
	csp->rpcServer = rpcServer;
	csp->rpcPort = rpcPort;
	csp->rpcUser = rpcUser;
	csp->rpcPass = rpcPass;
	csp->maxTreeDepth = -1;
	csp->cwType = CW_T_FILE;
}

CW_STATUS CWS_send_from_stream(FILE *stream, struct CWS_params *params, double *balanceDiff, char *resTxid);

CW_STATUS CWS_send_from_path(const char *path, struct CWS_params *params, double *balanceDiff, char *resTxid);

CW_STATUS CWS_send_dir_from_path(const char *path, struct CWS_params *params,  double *balanceDiff, char *resTxid);

#endif
