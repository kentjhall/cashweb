#ifndef __CASHWEBUNI_H__
#define __CASHWEBUNI_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

typedef uint16_t CW_STATUS;
#define CW_OK 0

// protocol version specific #defines
#define CW_P_VER 0

#define CW_T_FILE 0
#define CW_T_DIR 1

struct cwFileMetadata {
	uint32_t length;
	uint32_t depth;
	uint16_t type;
	uint16_t pVer;
};

static inline void initCwFileMetadata(struct cwFileMetadata *md, uint16_t cwFType) {
	md->length = 0;
	md->depth = 0;
	md->type = cwFType;
	md->pVer = CW_P_VER;
}

#define CW_MD_BYTES(md) sizeof(((struct cwFileMetadata *)0)->md)
#define CW_METADATA_BYTES (CW_MD_BYTES(length)+\
			   CW_MD_BYTES(depth)+\
			   CW_MD_BYTES(type)+\
			   CW_MD_BYTES(pVer))

// based on network rules
#define TX_RAW_DATA_BYTES 222
#define TX_DATA_BYTES (TX_RAW_DATA_BYTES-2)
#define TXID_BYTES 32

// _CHARS #defines for number of hex data chars
#define HEX_CHARS(bytes) (bytes*2)
#define CW_MD_CHARS(md) HEX_CHARS(CW_MD_BYTES(md))
#define CW_METADATA_CHARS HEX_CHARS(CW_METADATA_BYTES)
#define TX_RAW_DATA_CHARS HEX_CHARS(TX_RAW_DATA_BYTES)
#define TX_DATA_CHARS HEX_CHARS(TX_DATA_BYTES)
#define TXID_CHARS HEX_CHARS(TXID_BYTES)

static inline void die(char *e) { perror(e); exit(1); }

#endif
