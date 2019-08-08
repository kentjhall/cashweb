#ifndef __CASHWEBUNI_H__
#define __CASHWEBUNI_H__

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <jansson.h>

#define CW_P_VER 0

/* general status codes */
typedef int CW_STATUS;
#define CW_OK 0
#define CW_DATADIR_NO 1
#define CW_CALL_NO 2
#define CW_SYS_ERR 3

/* cashweb file types; MIMESET indicates that mimetype is to be interpreted */
typedef uint16_t CW_TYPE;
#define CW_T_FILE 0
#define CW_T_DIR 1
#define CW_T_MIMESET 2

/* cashweb scripting codes for nametag revisioning */
typedef uint8_t CW_OPCODE;
#define CW_OP_TERM ((CW_OPCODE)255)
#define CW_OP_NEXTREV (CW_OP_TERM-1)
#define CW_OP_PUSHTXID (CW_OP_TERM-2)
#define CW_OP_WRITEFROMTXID (CW_OP_TERM-3)
#define CW_OP_WRITEFROMNAMETAG (CW_OP_TERM-4)
#define CW_OP_WRITEFROMPREV (CW_OP_TERM-5)
#define CW_OP_PUSHUCHAR (CW_OP_TERM-6)
#define CW_OP_PUSHUSHORT (CW_OP_TERM-7)
#define CW_OP_PUSHUINT (CW_OP_TERM-8)
#define CW_OP_STOREFROMTXID (CW_OP_TERM-9)
#define CW_OP_STOREFROMNAMETAG (CW_OP_TERM-10)
#define CW_OP_STOREFROMPREV (CW_OP_TERM-11)
#define CW_OP_WRITESEGFROMSTORED (CW_OP_TERM-12)
#define CW_OP_WRITERANGEFROMSTORED (CW_OP_TERM-13)
#define CW_OP_DROPSTORED (CW_OP_TERM-14)
#define CW_OP_PATHREPLACE (CW_OP_TERM-15)
#define CW_OP_PUSHSTRX (CW_OP_PUSHSTR+1)
#define CW_OP_PUSHSTR ((CW_OPCODE)205)

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

/* calculates number of bytes for file metadata (single field and whole) */
#define CW_MD_BYTES(md_field) sizeof(((struct CW_file_metadata *)0)->md_field)
#define CW_METADATA_BYTES (CW_MD_BYTES(length)+\
			   CW_MD_BYTES(depth)+\
			   CW_MD_BYTES(type)+\
			   CW_MD_BYTES(pVer))

#define CW_NAMETAG_PREFIX "~"
#define CW_NAME_MAX_LEN (CW_TX_DATA_BYTES-CW_METADATA_BYTES-strlen(CW_NAMETAG_PREFIX)-2)
#define CW_REVISION_INPUT_VOUT 1

/* network rules constants */
#define CW_TX_RAW_DATA_BYTES 222
#define CW_TX_DATA_BYTES (CW_TX_RAW_DATA_BYTES-2)
#define CW_TXID_BYTES 32

/* data directory paths */
#define CW_INSTALL_DATADIR_PATH DATADIR"/"PACKAGE"/"
#define CW_DATADIR_MIMETYPES_PATH "CW_mimetypes/"

/* _CHARS for number of chars in hex str */
#define HEX_CHARS(bytes) (bytes*2)
#define CW_MD_CHARS(md_field) HEX_CHARS(CW_MD_BYTES(md_field))
#define CW_METADATA_CHARS HEX_CHARS(CW_METADATA_BYTES)
#define CW_TX_RAW_DATA_CHARS HEX_CHARS(CW_TX_RAW_DATA_BYTES)
#define CW_TX_DATA_CHARS HEX_CHARS(CW_TX_DATA_BYTES)
#define CW_TXID_CHARS HEX_CHARS(CW_TXID_BYTES)

/*
 * convenience function for checking validity of given txid
 */
inline bool CW_is_valid_txid(const char *txid) {
	size_t txidLen = strlen(txid);
	if (txidLen != CW_TXID_CHARS) { return false; }
	for (int i=0; i<txidLen; i++) { if (!isxdigit(txid[i])) { return false; } }
	return true;
}

/*
 * convenience function for checking validity of given name by cashweb protocol standards
 */
inline bool CW_is_valid_name(const char *name) { return strlen(name) <= CW_NAME_MAX_LEN; }

#endif
