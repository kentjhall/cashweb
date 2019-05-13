#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include <sys/stat.h>
#include <math.h>
#include "cashwebuni.h"

#define SEND_DATA_CMD "./send_data_tx.sh"

char *sendFile(FILE *fp);

#endif
