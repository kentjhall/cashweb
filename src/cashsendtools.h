#ifndef __CASHSENDTOOLS_H__
#define __CASHSENDTOOLS_H__

#include "cashwebuni.h"
#include <fts.h>
#include <libbitcoinrpc/bitcoinrpc.h>

/* cashsendtools error codes */
#define CWS_RPC_NO CW_SYS_ERR+1
#define CWS_INPUTS_NO CW_SYS_ERR+2
#define CWS_CONFIRMS_NO CW_SYS_ERR+3
#define CWS_FEE_NO CW_SYS_ERR+4
#define CWS_FUNDS_NO CW_SYS_ERR+5
#define CWS_RECOVERYFILE_NO CW_SYS_ERR+6
#define CWS_RPC_ERR CW_SYS_ERR+7

/* data directory paths for cashsendtools */
#define CW_DATADIR_REVISIONS_FILE "revision_locks.json"

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
	bool dirOmitIndex;
	size_t fragUtxos;
	FILE *saveDirStream;
	FILE *recoveryStream;
	const char *datadir;
};

/*
 * initializes struct CWS_params
 * rpcServer, rpcPort, rpcUser, and rpcPass are required on init
 */
static inline void init_CWS_params(struct CWS_params *csp,
				   const char *rpcServer, unsigned short rpcPort, const char *rpcUser, const char *rpcPass, FILE *recoveryStream) {
	csp->rpcServer = rpcServer;
	csp->rpcPort = rpcPort;
	csp->rpcUser = rpcUser;
	csp->rpcPass = rpcPass;
	csp->maxTreeDepth = -1;
	csp->cwType = CW_T_FILE;
	csp->dirOmitIndex = false;
	csp->fragUtxos = 1;
	csp->saveDirStream = NULL;
	csp->recoveryStream = recoveryStream;
	csp->datadir = CW_INSTALL_DATADIR_PATH;
}

/*
 * copies struct CWS_params from source to dest
 */
static inline void copy_CWS_params(struct CWS_params *dest, struct CWS_params *source) {
	dest->rpcServer = source->rpcServer;
	dest->rpcPort = source->rpcPort;
	dest->rpcUser = source->rpcUser;
	dest->rpcPass = source->rpcPass;
	dest->maxTreeDepth = source->maxTreeDepth;
	dest->cwType = source->cwType;
	dest->dirOmitIndex = source->dirOmitIndex;
	dest->fragUtxos = source->fragUtxos;
	dest->saveDirStream = source->saveDirStream;
	dest->recoveryStream = source->recoveryStream;
	dest->datadir = source->datadir;
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
 * wrapper for CWS_send_nametag that constructs script around linking to file at specified fTxid
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_standard_nametag(const char *name, const char *fTxid, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid);


/*
 * sends revision for nametag (using the appropriate UTXO specified by utxoTxid) with script
 * immutable is specified for whether or not to create/lock tiny UTXO for future revisions (should be false if so)
 * if not immutable, scriptStr needs to reflect this by containing CW_OP_NEXTREV (most likely first)
 * cost of send is written to fundsUsed
 * cwType specified in params is ignored in favor of CW_T_FILE
 */
CW_STATUS CWS_send_revision(const char *utxoTxid, const CW_OPCODE *script, size_t scriptSz, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * wrapper for CWS_send_revision that constructs script around completely replacing data with file at specified fTxid
 * will prepend script with CW_OP_NEXTREV if immutable is specified false
 */
CW_STATUS CWS_send_replace_revision(const char *utxoTxid, const char *fTxid, bool immutable, struct CWS_params *params, double *fundsUsed, char *resTxid);

/*
 * locks stored revisioning utxos (in data directory) via RPC
 * this will be done automatically by cashsendtools when sending,
   but this public function is available to protect revisioning utxos if bitcoind has been restarted (which apparently resets locked unspents)
   and funds need to be sent elsewhere
 */
CW_STATUS CWS_wallet_lock_revision_utxos(struct CWS_params *params);

/*
 * manually lock/unlock specified revisioning txid
 * name should be set if locking, or stored revisioning name will be empty string;
   if unlocking, name will be ignored (should be NULL), and any stored locks on specified txid (should really only be one) will be removed
 */
CW_STATUS CWS_set_revision_lock(const char *revTxid, bool unlock, const char *name, struct CWS_params *csp);

/*
 * determines revisioning txid by given name from what is stored in cashwebtools data directory and writes to revTxid
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
 * returns generic error message by error code
 */
const char *CWS_errno_to_msg(int errNo);

#endif
