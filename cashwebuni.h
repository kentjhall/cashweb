#ifndef __CASHWEBUNI_H__
#define __CASHWEBUNI_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define TX_DATA_BYTES 220
#define TXID_BYTES 32

#define TX_DATA_CHARS TX_DATA_BYTES*2
#define TXID_CHARS TXID_BYTES*2

static inline void die(char *e) { perror(e); exit(1); }

#endif
