#ifndef __CASHWEBUNI_H__
#define __CASHWEBUNI_H__

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <jansson.h>

#define CW_P_VER 0

// general status codes
typedef int CW_STATUS;
#define CW_OK 0
#define CW_DATADIR_NO 1
#define CW_CALL_NO 2
#define CW_SYS_ERR 3

// cashweb file types; MIMESET indicates that mimetype is to be interpreted
typedef uint16_t CW_TYPE;
#define CW_T_FILE 0
#define CW_T_DIR 1
#define CW_T_MIMESET 2

// cashweb nametag scripting codes
typedef uint8_t CW_OPCODE;
#define CW_OP_TERM (CW_OPCODE) 255
#define CW_OP_NEXTREV (CW_OPCODE) 254
#define CW_OP_PUSHTXID (CW_OPCODE) 253
#define CW_OP_WRITEFROMTXID (CW_OPCODE) 252

/*
 * file metadata to be stored in starting tx
 * length: file chain length
 * depth: file tree depth
 * type: file's CW type
 * pVer: protocol version under which file was sent
 */
struct CW_file_metadata {
	uint32_t length;
	uint32_t depth;
	CW_TYPE type;
	uint16_t pVer;
};

/*
 * initialize struct CW_file_metadata
 * file's cashweb type is required
 */
static inline void init_CW_file_metadata(struct CW_file_metadata *md, CW_TYPE cwFType) {
	md->length = 0;
	md->depth = 0;
	md->type = cwFType;
	md->pVer = CW_P_VER;
}

// calculates number of bytes for file metadata (single field and whole)
#define CW_MD_BYTES(md_field) sizeof(((struct CW_file_metadata *)0)->md_field)
#define CW_METADATA_BYTES (CW_MD_BYTES(length)+\
			   CW_MD_BYTES(depth)+\
			   CW_MD_BYTES(type)+\
			   CW_MD_BYTES(pVer))

#define CW_NAMETAG_PREFIX "~"

// network rules constants
#define CW_TX_RAW_DATA_BYTES 222
#define CW_TX_DATA_BYTES (CW_TX_RAW_DATA_BYTES-2)
#define CW_TXID_BYTES 32

// data directory paths
#define CW_INSTALL_DATADIR_PATH DATADIR"/"PACKAGE
#define CW_DATADIR_MIMETYPES_PATH "CW_mimetypes/"

// _CHARS for number of chars in hex str
#define HEX_CHARS(bytes) (bytes*2)
#define CW_MD_CHARS(md_field) HEX_CHARS(CW_MD_BYTES(md_field))
#define CW_METADATA_CHARS HEX_CHARS(CW_METADATA_BYTES)
#define CW_TX_RAW_DATA_CHARS HEX_CHARS(CW_TX_RAW_DATA_BYTES)
#define CW_TX_DATA_CHARS HEX_CHARS(CW_TX_DATA_BYTES)
#define CW_TXID_CHARS HEX_CHARS(CW_TXID_BYTES)

#endif
