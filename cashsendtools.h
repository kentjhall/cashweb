#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include <sys/stat.h>
#include "cashwebuni.h"

#define SEND_DATA_CMD "./send_data_tx.sh"
#define CLI_LINE_BUF 10
#define CHECK_UTXOS_CMD "bitcoin-cli listunspent | jq \'. | length\'"
#define CHECK_BALANCE_CMD "echo $(bc -l <<< \"$(bitcoin-cli getbalance) + $(bitcoin-cli getunconfirmedbalance)\")"
#define ERR_WAIT_CYCLE 5

// send file to blockchain
char *sendFile(FILE *fp, int maxTreeDepth);

// check client balance via CHECK_BALANCE_CMD shell command
double checkBalance();

#endif
