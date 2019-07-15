#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include <sys/stat.h>
#include <fts.h>
#include <libbitcoinrpc/bitcoinrpc.h>
#include "cashwebuni.h"

#define CWS_RPC_NO 1
#define CWS_CONFIRMS_NO 2
#define CWS_FEE_NO 3
#define CWS_FUNDS_NO 4
#define CWS_RPC_ERR 5

// send file to blockchain
// specify maxTreeDepth, will be chained at top level
char *sendFile(const char *filePath, int cwType, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff);

// send directory to blockchain
// specify maxTreeDepth for all files
char *sendDir(const char *dirPath, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff);

#endif
