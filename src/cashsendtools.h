#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include "cashwebuni.h"

/* cashsendtools status codes */
#define CWS_RPC_NO CW_SYS_ERR+1
#define CWS_INPUTS_NO CW_SYS_ERR+2
#define CWS_CONFIRMS_NO CW_SYS_ERR+3
#define CWS_FEE_NO CW_SYS_ERR+4
#define CWS_FUNDS_NO CW_SYS_ERR+5
#define CWS_RECOVERYFILE_NO CW_SYS_ERR+6
#define CWS_RPC_ERR CW_SYS_ERR+7

/* data directory paths for cashsendtools */
#define CW_DATADIR_REVISIONS_FILE "revision_locks.json"

/* can be set to redirect cashsendtools error logging; defaults to stderr */
extern FILE *CWS_err_stream;

/* can be set to redirect cashsendtools progress logging; defaults to stderr */
extern FILE *CWS_log_stream;

/*
 * convenience pack for passing common qualifers to revision functions
 */
struct CWS_revision_pack {
	bool immutable;
	const char *pathToReplace;
	const char *pathReplacement;
	const char *transferAddr;
};

/*
 * initializes struct CWS_revision_pack
 */
static inline void init_CWS_revision_pack(struct CWS_revision_pack *rvp) {
        rvp->immutable = false;
        rvp->pathToReplace = NULL;
        rvp->pathReplacement = NULL;
        rvp->transferAddr = NULL;
}

/*
 * params for sending
 * rpcServer: RPC address
 * rpcPort: RPC port
 * rpcUser: RPC username
 * rpcPass: RPC password 
 * maxTreeDepth: maximum depth for file tree (will send as chained tree if hit);
 		 determines the chunk size the file is downloaded in
 * cwType: the file's cashweb type (set to CW_T_MIMESET if the mimetype should be interpreted by extension)
 * dirOmitIndex: if sending directory, can be specified to not send index (i.e. to send collection of files)
 * revToAddr: specifies an address to force last tiny change output to when revisioning (for transferring ownership);
 	      needn't be specified if using struct CWS_revision_pack (equivalent to transferAddr)
 * fragUtxos: specifies the number of UTXOs to create for file send in advance; best to have this handled by CWS_estimate_cost function
 *	       leave set at 1 to have cashsendtools calculate this, or 0 to not fragment any UTXOs
 * saveDirStream: optionally save directory index data to specified stream;
 		  if dirOmitIndex is true, this will still save but will not be sent 
 * recoveryStream: where to save recovery data in case of failure
 * datadir: specify data directory path for cashwebtools;
 	    can be left as NULL if cashwebtools is properly installed on system with 'make install'
 */
struct CWS_params {
	const char *rpcServer;
	unsigned short rpcPort;
	const char *rpcUser;
	const char *rpcPass;
	int maxTreeDepth;
	CW_TYPE cwType;
	const char *revToAddr;
	size_t fragUtxos;
	bool dirOmitIndex;
	FILE *saveDirStream;
	FILE *recoveryStream;
	const char *datadir;
};

/*
 * initializes struct CWS_params
 * rpcServer, rpcPort, rpcUser, and rpcPass are required on init
 */
void init_CWS_params(struct CWS_params *csp, const char *rpcServer, unsigned short rpcPort, const char *rpcUser, const char *rpcPass, FILE *recoveryStream);

/*
 * copies struct CWS_params from source to dest
 */
void copy_CWS_params(struct CWS_params *dest, struct CWS_params *source);

/*
 * determines if given struct CWS_revision_pack contains any new (non-default) information
 */
static inline bool is_default_CWS_revision_pack(struct CWS_revision_pack *rvp) {
        struct CWS_revision_pack def;
        init_CWS_revision_pack(&def);
        return rvp->immutable == def.immutable &&
               rvp->pathToReplace == def.pathToReplace &&
               rvp->pathReplacement == def.pathReplacement &&
               rvp->transferAddr == def.transferAddr;
}

/*
 * sends file/directory at specified path as per options set in params
 * if directory, all contained files are sent with given params; this includes maxTreeDepth and cwType
 * set cwType to CW_T_MIMESET for file mimetypes to be interpreted by extension
 * if sending directory index, will always be sent as CW_T_DIR, regardless of params
 * full cost of the send is written to fundsUsed; on failure, the cost of any irrecoverable progress is written to fundsLost;
   if not needed, both/either can be set to NULL
 * resultant txid is written to resTxid
 */
CW_STATUS CWS_send_from_path(const char *path, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid);

/*
 * estimates cost for sending file/directory from path with given params
 * writes to costEstimate
 */
CW_STATUS CWS_estimate_cost_from_path(const char *path, struct CWS_params *params, size_t *txCount, double *costEstimate);

/*
 * sends file from stream as per options set in params
 * cashsendtools does not ascertain mimetype when sending from stream, so if type is set to CW_T_MIMESET in params,
   will default to CW_T_FILE
 * full cost of the send is written to fundsUsed, and on failure, the cost of any irrecoverable progress is written to fundsLost;
   if not needed, both/either can be set to NULL
 * resultant txid is written to resTxid
 */
CW_STATUS CWS_send_from_stream(FILE *stream, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid);

/*
 * estimates cost for sending file from stream with given params
 * writes to costEstimate
 */
CW_STATUS CWS_estimate_cost_from_stream(FILE *stream, struct CWS_params *params, size_t *txCount, double *costEstimate);

/*
 * interprets cashsendtools recovery data from a failed send and attempts to finish the send
 * params is included for RPC params, but cwType/maxTreeDepth will be ignored in favor of what is stored in recovery data
 * full cost of the send is written to fundsUsed; on failure, the cost of any irrecoverable progress is written to fundsLost;
   if not needed, both/either can be set to NULL
 * resultant txid is written to resTxid
 */
CW_STATUS CWS_send_from_recovery_stream(FILE *recoveryStream, struct CWS_params *params, double *fundsUsed, double *fundsLost, char *resTxid);

/*
 * estimates cost for sending from cashsendtools recovery data with given params
 * writes to costEstimate
 */
CW_STATUS CWS_estimate_cost_from_recovery_stream(FILE *recoveryStream, struct CWS_params *params, size_t *txCount, double *costEstimate);

/*
 * claim a cashweb protocol name with script
 * recommended to check for existence of name first; cashgettools should only use earliest claim for any name
 * immutable is specified for whether or not to create/lock tiny UTXO for future revisions (should be false if so)
 * if not immutable, scriptStr needs to reflect this by containing CW_OP_NEXTREV (most likely first)
 * cost of send is written to fundsUsed
 * cwType specified in params is ignored in favor of CW_T_FILE
 */
CW_STATUS CWS_send_nametag(const char *name, const CW_OPCODE *script, size_t scriptSz, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_nametag that constructs script around linking to file at specified identifier attachId
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_standard_nametag(const char *name, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);


/*
 * sends revision for nametag (using the appropriate UTXO specified by utxoTxid) with script
 * immutable is specified for whether or not to create/lock tiny UTXO for future revisions (should be false if so)
 * if not immutable, scriptStr needs to reflect this by containing CW_OP_NEXTREV (most likely first)
 * cost of send is written to fundsUsed
 * cwType specified in params is ignored in favor of CW_T_FILE
 */
CW_STATUS CWS_send_revision(const char *utxoTxid, const CW_OPCODE *script, size_t scriptSz, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script around completely replacing data with file at specified identifier attachId
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_replace_revision(const char *utxoTxid, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script around prepending existing data with file at specified identifier attachId
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_prepend_revision(const char *utxoTxid, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script around appending to existing data with file at specified identifier attachId
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_append_revision(const char *utxoTxid, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script around inserting into existing data at byte position bytePos with file at specified identifier attachId
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_insert_revision(const char *utxoTxid, size_t bytePos, const char *attachId, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script around "deleting" (skipping) bytesToDel bytes of data starting from position startPos
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_delete_revision(const char *utxoTxid, size_t startPos, size_t bytesToDel, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script purely around contents of given struct CWS_revision_pack, with no real content added/changed
 * if script ends up completely empty, will put CW_OP_PUSHNO as whole script
 * may be useful for transferring ownership of nametag, as well as rendering a nametag immutable or redirecting path for existing directory
 */
CW_STATUS CWS_send_empty_revision(const char *utxoTxid, struct CWS_revision_pack *rvp, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * constructs script for pushing unsigned integer uint per appropriate number of bytes and writes to scriptStr
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 */
void CWS_gen_script_push_int(uint32_t val, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * constructs script for pushing string str by its length and writes to scriptPtr
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 */
void CWS_gen_script_push_str(const char *str, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * constructs script for getting/writing from given nametag
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 */
void CWS_gen_script_writefrom_nametag(const char *name, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * constructs script for getting/writing from given txid
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 * returns true on success, false on invalid hex for txid; shouldn't need to error-check if format of txid string has been verified
 */
bool CWS_gen_script_writefrom_txid(const char *txid, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * constructs script for getting/writing from given identifier;
   can be nametag id or txid, but NOT nametag revision id (e.g. 0~coolcashwebname)
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 * returns true on success, false on invalid identifier
 */ 
bool CWS_gen_script_writefrom_id(const char *id, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * constructs script for path replacement (for script pointing to directory, suggests replacement of path 'toReplace' with 'replacement')
 * paths may be prepended with '/'; shouldn't make a difference unless replacing lone slash (just '/', which is valid)
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 */
void CWS_gen_script_pathlink(const char *toReplace, const char *replacement, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * constructs standard script beginning from given struct CWS_revision_pack (pertaining to immutability, path replacement)
 * writes transferAddr to params (here for convenience, doesn't affect script) for handling ownership transfer
 * reads/writes to scriptSz as going (for tracking position in script)
 * must ensure adequate space is allocated to scriptPtr memory location
 */
void CWS_gen_script_standard_start(struct CWS_revision_pack *rvp, struct CWS_params *params, CW_OPCODE *scriptPtr, size_t *scriptSz);

/*
 * locks stored revisioning utxos (in data directory) via RPC
 * this will be done automatically by cashsendtools when sending,
   but this public function is available to protect revisioning utxos if bitcoind has been restarted (which apparently resets locked unspents)
   and funds need to be sent elsewhere
 */
CW_STATUS CWS_wallet_lock_revision_utxos(struct CWS_params *params);

/*
 * manually lock/unlock specified name/txid for revisioning
 * when locking, both name and revTxid are required; 
   when unlocking, either name or revTxid is required; if both provided, will first attempt to match name;
   if conditions aren't met, behavior is undefined
 * returns CW_CALL_NO if locking name/txid that is already locked or unlocking one that isn't, or otherwise appropriate status code;
   conversely, will still return CW_OK when unlocking name/txid that is not already locked
 */
CW_STATUS CWS_set_revision_lock(const char *name, const char *revTxid, bool unlock, struct CWS_params *csp);

/*
 * determines revisioning txid by given name from what is stored in cashwebtools data directory and writes to revTxid
 * can be used to determine if name is locked; pass NULL for revTxid if write not needed, and check for status CW_CALL_NO
 * returns CW_CALL_NO if name goes unmatched, or otherwise appropriate status code
 */
CW_STATUS CWS_get_stored_revision_txid_by_name(const char *name, struct CWS_params *csp, char *revTxid);

/*
 * determines protocol-specific cashweb type value for given filename/extension by mimetype and copies to given struct CWS_params
 * fname can be full filename or just extension
 * this function is public in case the user wants to force a mimetype other than what matches the file extension,
   or more likely, if a mimetype needs to be set when sending from stream
 * if type is not matched in mime.types, cwType is set to CW_T_FILE; if set mimetype is critical, this should be checked
 * uses datadir path stored in params to find cashweb protocol-specific mime.types;
   if this is NULL, will set to proper cashwebtools system install data directory
 */
CW_STATUS CWS_set_cw_mime_type_by_extension(const char *fname, struct CWS_params *csp);

/*
 * reads directory index JSON data and writes as protocol-compliant index data to given stream
 */
CW_STATUS CWS_dirindex_json_to_raw(FILE *indexJsonFp, FILE *indexFp);

/*
 * returns generic error message by error code
 */
const char *CWS_errno_to_msg(int errNo);

#endif
