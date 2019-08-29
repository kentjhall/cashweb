#ifndef __CASHWEBUNI_H__
#define __CASHWEBUNI_H__

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
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
#define CW_OP_PUSHCHAR (CW_OP_TERM-6)
#define CW_OP_PUSHSHORT (CW_OP_TERM-7)
#define CW_OP_PUSHINT (CW_OP_TERM-8)
#define CW_OP_STOREFROMTXID (CW_OP_TERM-9)
#define CW_OP_STOREFROMNAMETAG (CW_OP_TERM-10)
#define CW_OP_STOREFROMPREV (CW_OP_TERM-11)
#define CW_OP_SEEKSTORED (CW_OP_TERM-12)
#define CW_OP_WRITEFROMSTORED (CW_OP_TERM-13)
#define CW_OP_WRITESOMEFROMSTORED (CW_OP_TERM-14)
#define CW_OP_DROPSTORED (CW_OP_TERM-16)
#define CW_OP_WRITEPATHLINK (CW_OP_TERM-15)
#define CW_OP_PUSHSTRX (CW_OP_PUSHSTR+1)
#define CW_OP_PUSHSTR ((CW_OPCODE)205)
#define CW_OP_PUSHNO ((CW_OPCODE)0)

/* whence/direction constants for script code CW_OP_SEEKSTORED */
#define CW_SEEK_BEG 0
#define CW_SEEK_CUR 1
#define CW_SEEK_CUR_NEG 2
#define CW_SEEK_END_NEG 3

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

/*
 * copies struct CW_file_metadata from source to dest
 */
static inline void copy_CW_file_metadata(struct CW_file_metadata *dest, struct CW_file_metadata *source) {
	dest->length = source->length;
	dest->depth = source->depth;
	dest->type = source->type;
	dest->pVer = source->pVer;
}

/* calculates number of bytes for file metadata (single field and whole) */
#define CW_MD_BYTES(md_field) sizeof(((struct CW_file_metadata *)0)->md_field)
#define CW_METADATA_BYTES (CW_MD_BYTES(length)+\
			   CW_MD_BYTES(depth)+\
			   CW_MD_BYTES(type)+\
			   CW_MD_BYTES(pVer))

/* nametag protocol constants */
#define CW_NAMETAG_PREFIX "~"
#define CW_NAMETAG_PREFIX_LEN (sizeof(CW_NAMETAG_PREFIX)-1)
#define CW_NAME_MAX_LEN (CW_TX_DATA_BYTES-CW_METADATA_BYTES-CW_NAMETAG_PREFIX_LEN-2)
#define CW_REV_STR_IMPL(rev) #rev
#define CW_REV_STR(rev) CW_REV_STR_IMPL(rev)
#define CW_REV_STR_MAX_LEN (sizeof(CW_REV_STR(INT_MAX))-1)
#define CW_NAMETAG_ID_MAX_LEN (CW_NAME_MAX_LEN+CW_NAMETAG_PREFIX_LEN+CW_REV_STR_MAX_LEN)

/* revisioning protocol constants */
#define CW_REVISION_INPUT_VOUT 1
#define CW_REV_LATEST -1

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
 * convenience function for checking validity of given txid (hex string of length TXID_CHARS)
 */
static inline bool CW_is_valid_txid(const char *txid) {
	size_t txidLen = strlen(txid);
	if (txidLen != CW_TXID_CHARS) { return false; }
	for (int i=0; i<txidLen; i++) { if (!isxdigit(txid[i])) { return false; } }
	return true;
}

/*
 * convenience function for checking validity of given name by cashweb protocol standards (should not include nametag ID prefix)
 */
static inline bool CW_is_valid_name(const char *name) { size_t nameLen = strlen(name); return !strchr(name, '/') && nameLen <= CW_NAME_MAX_LEN && nameLen > 0; }

/*
 * checks if given string is a valid nametag id by cashweb protocol standards for prefix and length
 * if so, writes revision and name pointer (points to location in passed id string) to passed memory locations (can be NULL if not desired)
 */
static inline bool CW_is_valid_nametag_id(const char *id, int *rev, const char **name) {
	char *pre;
	if ((pre = strstr(id, CW_NAMETAG_PREFIX))) {
		char *nameStr = pre + CW_NAMETAG_PREFIX_LEN;
		size_t revLen = pre - id;
		if (!CW_is_valid_name(nameStr)) { return false; }
		if (revLen > CW_REV_STR_MAX_LEN) { return false; }

		if (rev) {
			if (revLen > 0) {
				char revStr[revLen+1]; revStr[0] = 0;
				strncat(revStr, id, revLen);
				*rev = atoi(revStr);
			} else { *rev = CW_REV_LATEST; }
		}
		if (name) { *name = nameStr; }
		return true;
	}
	return false;
}

/*
 * checks if given string is a valid path id by cashweb protocol standards (i.e. presence of '/' and length of contained id);
   does NOT check for validity of contained id (whether it be nametag id or txid) beyond its length
 * if valid, copies id and path pointer (points to location in passed pathId string) to passed memory locations (can be NULL if not desired)
 */
static inline bool CW_is_valid_path_id(const char *pathId, char *id, const char **path) {
	char *idPath;
	if ((idPath = strstr(pathId, "/"))) {
		size_t idLen = idPath - pathId;
		if (idLen > CW_NAMETAG_ID_MAX_LEN || idLen < 1) { return false; }

		if (id) { id[0] = 0; strncat(id, pathId, idLen); }
		if (path) { *path = idPath; }
		return true;
	}
	return false;
}

/*
 * checks if given string is valid as any sort of cashweb id by protocol standards
 * does not ascertain any information about it; will need to be specific with above functions if this is desired
 */
static inline bool CW_is_valid_cashweb_id(const char *id) {
	char idEnc[CW_NAMETAG_ID_MAX_LEN+1];
	return (CW_is_valid_path_id(id, idEnc, NULL) && CW_is_valid_cashweb_id(idEnc)) || CW_is_valid_nametag_id(id, NULL, NULL) || CW_is_valid_txid(id);
}

/*
 * constructs valid cashweb nametag ID from name and revision
 */
static inline void CW_construct_nametag_id(const char *name, int revision, char (*nametagId)[CW_NAMETAG_ID_MAX_LEN+1]) {
	(*nametagId)[0] = 0;
	if (revision >= 0) { snprintf(*nametagId, sizeof(*nametagId), "%d", revision); }
	strcat(*nametagId, CW_NAMETAG_PREFIX);
	strcat(*nametagId, name);
}

#endif
