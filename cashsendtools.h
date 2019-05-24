#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include <sys/stat.h>
#include "cashwebuni.h"

#define SEND_DATA_CMD "./send_data_tx.sh"
#define CHECK_UTXOS_CMD "bitcoin-cli listunspent | jq \'. | length\'"
#define WAIT_CYCLE 5

char *sendFile(FILE *fp, int maxTreeDepth);

#endif
