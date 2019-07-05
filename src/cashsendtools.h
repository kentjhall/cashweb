#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include <sys/stat.h>
#include <fts.h>
#include <jansson.h>
#include <libbitcoinrpc/bitcoinrpc.h>
#include "cashwebuni.h"

// send file to blockchain
// specify maxTreeDepth, will be chained at top level
char *sendFile(const char *filePath, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff);

// send directory to blockchain
// specify maxTreeDepth for all files
char *sendDir(const char *dirPath, int maxTreeDepth, bitcoinrpc_cl_t *rpcCli, double *balanceDiff);

#endif
