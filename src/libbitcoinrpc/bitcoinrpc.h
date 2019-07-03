/*
   The MIT License (MIT)
   Copyright (c) 2016 Marek Miller

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef BITCOINRPC_H_51fe7847_aafe_4e78_9823_eff094a30775
#define BITCOINRPC_H_51fe7847_aafe_4e78_9823_eff094a30775

#include <jansson.h>


/* Name and version */
#define BITCOINRPC_LIBNAME "bitcoinrpc"

#define BITCOINRPC_VERSION "0.2.0"
#define BITCOINTPC_VERSION_MAJOR 0
#define BITCOINTPC_VERSION_MINOR 1
#define BITCOINTPC_VERSION_PATCH 0
#define BITCOINRPC_VERSION_HEX 0x000100


/* Defalut parameters of a RPC client */
#define BITCOINRPC_USER_DEFAULT ""
#define BITCOINRPC_PASS_DEFAULT ""
#define BITCOINRPC_ADDR_DEFAULT "127.0.0.1"
#define BITCOINRPC_PORT_DEFAULT 8332

/*
   Maximal length of a string that holds a client's parameter
   (user name, password or address), including the terminating '\0' character.
 */
#define BITCOINRPC_PARAM_MAXLEN 65

/*
   Maximal length of the server url:
   "http://%s:%d" = 2*BITCOINRPC_PARAM_MAXLEN + 13
 */
#define BITCOINRPC_URL_MAXLEN 143

/* Maximal length of an error message */
#define BITCOINRPC_ERRMSG_MAXLEN 1000

// /* satoshi typedef: one hundred millionth of a bitcoin */
// typedef unsigned long long int bitcoinrpc_satoshi_t;
//
// /* How many satoshi is in one bitcoin */
// #define BITCOINRPC_SATOSHI_BTC 1000000
// #define BITCOINRPC_DOUBLE_TO_SATOSHI(d) (bitcoinrpc_satoshi_t)d * BITCOINRPC_SATOSHI_BTC
// #define BITCOINRPC_SATOSHI_TO_DOUBLE(n) (double)n / BITCOINRPC_SATOSHI_BTC

/* Error codes */
typedef enum {
  BITCOINRPCE_OK,                 /* Success */
  BITCOINRPCE_ALLOC,              /* cannot allocate more memory */
  BITCOINRPCE_ARG,                /* wrong argument, e.g. NULL */
  BITCOINRPCE_BUG,                /* a bug in the library (please report) */
  BITCOINRPCE_CHECK,              /* see: bitcoinrpc_resp_check() */
  BITCOINRPCE_CON,                /* connection error */
  BITCOINRPCE_CURLE,              /* libcurl returned some error */
  BITCOINRPCE_ERR,                /* unspecific error */
  BITCOINRPCE_JSON,               /* error parsing json data */
  BITCOINRPCE_SERV                /* Bitcoin server returned error */
} BITCOINRPCEcode;


/* RPC methods */
typedef enum {
  BITCOINRPC_METHOD_NONSTANDARD,               /* reserved for possible */
                                               /* user extensions       */

  BITCOINRPC_METHOD_GETBESTBLOCKHASH,          /* getbestblockhash */
  BITCOINRPC_METHOD_GETBLOCK,                  /* getblock */
  BITCOINRPC_METHOD_GETBLOCKCHAININFO,         /* getblockchaininfo */
  BITCOINRPC_METHOD_GETBLOCKCOUNT,             /* getblockcount */
  BITCOINRPC_METHOD_GETBLOCKHASH,              /* getblockhash */
  BITCOINRPC_METHOD_GETCHAINTIPS,              /* getchaintips */
  BITCOINRPC_METHOD_GETDIFFICULTY,             /* getdifficulty */
  BITCOINRPC_METHOD_GETMEMPOOLINFO,            /* getmempoolinfo */
  BITCOINRPC_METHOD_GETRAWMEMPOOL,             /* getrawmempool */
  BITCOINRPC_METHOD_GETTXOUT,                  /* gettxout */
  BITCOINRPC_METHOD_GETTXOUTPROOF,             /* gettxoutproof */
  BITCOINRPC_METHOD_GETTXOUTSETINFO,           /* gettxoutsetinfo */
  BITCOINRPC_METHOD_VERIFYCHAIN,               /* verifychain */
  BITCOINRPC_METHOD_VERIFYTXOUTPROOF,          /* verifytxoutproof */

/* Control RPCs */
  BITCOINRPC_METHOD_GETINFO,                   /* getinfo */
  BITCOINRPC_METHOD_HELP,                      /* help */
  BITCOINRPC_METHOD_STOP,                      /* stop */

/* Generating RPCs */
  BITCOINRPC_METHOD_GENERATE,                  /* generate */
  BITCOINRPC_METHOD_GETGENERATE,               /* getgenerate */
  BITCOINRPC_METHOD_SETGENERATE,               /* setgenerate */

/* Mining RPCs */
  BITCOINRPC_METHOD_GETBLOCKTEMPLATE,          /* getblocktemplate */
  BITCOINRPC_METHOD_GETMININGINFO,             /* getmininginfo */
  BITCOINRPC_METHOD_GETNETWORKHASHPS,          /* getnetworkhashps */
  BITCOINRPC_METHOD_PRIORITISETRANSACTION,     /* prioritisetransaction */
  BITCOINRPC_METHOD_SUBMITBLOCK,               /* submitblock */

/* Network RPCs */
  BITCOINRPC_METHOD_ADDNODE,                   /* addnode */
  BITCOINRPC_METHOD_GETADDEDNODEINFO,          /* getaddednodeinfo */
  BITCOINRPC_METHOD_GETCONNECTIONCOUNT,        /* getconnectioncount */
  BITCOINRPC_METHOD_GETNETTOTALS,              /* getnettotals */
  BITCOINRPC_METHOD_GETNETWORKINFO,            /* getnetworkinfo */
  BITCOINRPC_METHOD_GETPEERINFO,               /* getpeerinfo */
  BITCOINRPC_METHOD_PING,                      /* ping */

/* Raw transaction RPCs */
  BITCOINRPC_METHOD_CREATERAWTRANSACTION,      /* createrawtransaction */
  BITCOINRPC_METHOD_DECODERAWTRANSACTION,      /* decoderawtransaction */
  BITCOINRPC_METHOD_DECODESCRIPT,              /* decodescript */
  BITCOINRPC_METHOD_GETRAWTRANSACTION,         /* getrawtransaction */
  BITCOINRPC_METHOD_SENDRAWTRANSACTION,        /* sendrawtransaction */
  BITCOINRPC_METHOD_SIGNRAWTRANSACTION,        /* signrawtransaction */

/* Utility RPCs */
  BITCOINRPC_METHOD_CREATEMULTISIG,            /* createmultisig */
  BITCOINRPC_METHOD_ESTIMATEFEE,               /* estimatefee */
  BITCOINRPC_METHOD_ESTIMATEPRIORITY,          /* estimatepriority */
  BITCOINRPC_METHOD_VALIDATEADDRESS,           /* validateaddress */
  BITCOINRPC_METHOD_VERIFYMESSAGE,             /* verifymessage */

/* Wallet RPCs */
  BITCOINRPC_METHOD_ADDMULTISIGADDRESS,        /* addmultisigaddress */
  BITCOINRPC_METHOD_BACKUPWALLET,              /* backupwallet */
  BITCOINRPC_METHOD_DUMPPRIVKEY,               /* dumpprivkey */
  BITCOINRPC_METHOD_DUMPWALLET,                /* dumpwallet */
  BITCOINRPC_METHOD_ENCRYPTWALLET,             /* encryptwallet */
  BITCOINRPC_METHOD_GETACCOUNTADDRESS,         /* getaccountaddress */
  BITCOINRPC_METHOD_GETACCOUNT,                /* getaccount */
  BITCOINRPC_METHOD_GETADDRESSESBYACCOUNT,     /* getaddressesbyaccount */
  BITCOINRPC_METHOD_GETBALANCE,                /* getbalance */
  BITCOINRPC_METHOD_GETNEWADDRESS,             /* getnewaddress */
  BITCOINRPC_METHOD_GETRAWCHANGEADDRESS,       /* getrawchangeaddress */
  BITCOINRPC_METHOD_GETRECEIVEDBYACCOUNT,      /* getreceivedbyaccount */
  BITCOINRPC_METHOD_GETRECEIVEDBYADDRESS,      /* getreceivedbyaddress */
  BITCOINRPC_METHOD_GETTRANSACTION,            /* gettransaction */
  BITCOINRPC_METHOD_GETUNCONFIRMEDBALANCE,     /* getunconfirmedbalance */
  BITCOINRPC_METHOD_GETWALLETINFO,             /* getwalletinfo */
  BITCOINRPC_METHOD_IMPORTADDRESS,             /* importaddress */
  BITCOINRPC_METHOD_IMPORTPRIVKEY,             /* importprivkey */
  BITCOINRPC_METHOD_IMPORTWALLET,              /* importwallet */
  BITCOINRPC_METHOD_KEYPOOLREFILL,             /* keypoolrefill */
  BITCOINRPC_METHOD_LISTACCOUNTS,              /* listaccounts */
  BITCOINRPC_METHOD_LISTADDRESSGROUPINGS,      /* listaddressgroupings */
  BITCOINRPC_METHOD_LISTLOCKUNSPENT,           /* listlockunspent */
  BITCOINRPC_METHOD_LISTRECEIVEDBYACCOUNT,     /* listreceivedbyaccount */
  BITCOINRPC_METHOD_LISTRECEIVEDBYADDRESS,     /* listreceivedbyaddress */
  BITCOINRPC_METHOD_LISTSINCEBLOCK,            /* listsinceblock */
  BITCOINRPC_METHOD_LISTTRANSACTIONS,          /* listtransactions */
  BITCOINRPC_METHOD_LISTUNSPENT,               /* listunspent */
  BITCOINRPC_METHOD_LOCKUNSPENT,               /* lockunspent */
  BITCOINRPC_METHOD_MOVE,                      /* move */
  BITCOINRPC_METHOD_SENDFROM,                  /* sendfrom */
  BITCOINRPC_METHOD_SENDMANY,                  /* sendmany */
  BITCOINRPC_METHOD_SENDTOADDRESS,             /* sendtoaddress */
  BITCOINRPC_METHOD_SETACCOUNT,                /* setaccount */
  BITCOINRPC_METHOD_SETTXFEE,                  /* settxfee */
  BITCOINRPC_METHOD_SIGNMESSAGE,               /* signmessage */
  BITCOINRPC_METHOD_WALLETLOCK,                /* walletlock */
  BITCOINRPC_METHOD_WALLETPASSPHRASE,          /* walletpassphrase */
  BITCOINRPC_METHOD_WALLETPASSPHRASECHANGE     /* walletpassphrasechange */
} BITCOINRPC_METHOD;

/* ---------------- bitcoinrpc_err --------------------- */
struct bitcoinrpc_err {
  BITCOINRPCEcode code;
  char msg[BITCOINRPC_ERRMSG_MAXLEN];
};

typedef
struct bitcoinrpc_err
bitcoinrpc_err_t;


/* --------------- bitcoinrpc_global ------------------- */
/*
   The global initialisation function.
   Please call this function from your main thread before any other call.
 */
BITCOINRPCEcode
bitcoinrpc_global_init(void);

/*
   The global cleanup function.
   Please call this function at the end of your program to collect library's
   internal garbage.
 */
BITCOINRPCEcode
bitcoinrpc_global_cleanup(void);

/*
   Set a memory allocating function for the library routines.
   (the default is just standard malloc() ).
 */
BITCOINRPCEcode
bitcoinrpc_global_set_allocfunc(void * (*const f)(size_t size));

/*
   Set a memory freeing function for the library routines.
   (the default is just standard free() ).
 */
BITCOINRPCEcode
bitcoinrpc_global_set_freefunc(void(*const f) (void *ptr));


/* -------------bitcoinrpc_cl --------------------- */
struct bitcoinrpc_cl;

typedef
struct bitcoinrpc_cl
bitcoinrpc_cl_t;

/*
   Initialise a new RPC client with default values: BITCOINRPC_*_DEFAULT.
   Return NULL in case of error.
 */
bitcoinrpc_cl_t*
bitcoinrpc_cl_init(void);


/*
   Initialise and set some parameters (may not be NULL; in that case the
   function returns NULL as well). The parameter values are copied,
   so the original pointers are no longer needed. At most
   BITCOINRPC_PARAM_MAXLEN chars are copied to store a parameter.
 */
bitcoinrpc_cl_t*
bitcoinrpc_cl_init_params(const char* user, const char* pass,
                          const char* addr, const unsigned int port);

/* Free the handle. */
BITCOINRPCEcode
bitcoinrpc_cl_free(bitcoinrpc_cl_t *cl);

/*
   Copy value to buf. The buffer is assumed to contain at least
   BITCOINRPC_PARAM_MAXLEN chars. At most BITCOINRPC_PARAM_MAXLEN chars are copied.
 */
BITCOINRPCEcode
bitcoinrpc_cl_get_user(bitcoinrpc_cl_t *cl, char *buf);

BITCOINRPCEcode
bitcoinrpc_cl_get_pass(bitcoinrpc_cl_t *cl, char *buf);

BITCOINRPCEcode
bitcoinrpc_cl_get_addr(bitcoinrpc_cl_t *cl, char *buf);

BITCOINRPCEcode
bitcoinrpc_cl_get_port(bitcoinrpc_cl_t *cl, unsigned int *bufi);

/*
   Copy value to buf. The buffer is assumed to contain at least
   BITCOINRPC_URL_MAXLEN chars. At most BITCOINRPC_URL_MAXLEN chars are copied.
 */
BITCOINRPCEcode
bitcoinrpc_cl_get_url(bitcoinrpc_cl_t *cl, char *buf);

/* ------------- bitcoinrpc_method --------------------- */
struct bitcoinrpc_method;

typedef
struct bitcoinrpc_method
bitcoinrpc_method_t;

/* Initialise a new method without params */
bitcoinrpc_method_t *
bitcoinrpc_method_init(const BITCOINRPC_METHOD m);


/*
   The argument json_t params to functions below is always copied
   to library internals and the original pointer is no longer needed
   after the function returns.  It is the obligation of the user to free
   the original pointer by decreasing its reference count (see jansson
   library documentation).
 */

/*
   Initialise a new method with json_t array as params.
   If params == NULL, this is the same as bitcoinrpc_method_init.
 */
bitcoinrpc_method_t *
bitcoinrpc_method_init_params(const BITCOINRPC_METHOD m,
                              json_t * const params);

/* Destroy the method */
BITCOINRPCEcode
bitcoinrpc_method_free(bitcoinrpc_method_t *method);

/* Set a new json object as method parameters */
BITCOINRPCEcode
bitcoinrpc_method_set_params(bitcoinrpc_method_t *method, json_t *params);

/* Get a deepcopy of the method's parameters and store it in params */
BITCOINRPCEcode
bitcoinrpc_method_get_params(bitcoinrpc_method_t *method, json_t **params);

/*
   Set a custom name for the method
   Works only if method is set to BITCOINRPC_METHOD_NONSTANDARD,
   otherwise returns BITCOINRPCE_ERR.
   The pointer to str is copied, not the string itself. It is the obligation of
   the user, to keep the str available and free it, when no longer needed.
 */
BITCOINRPCEcode
bitcoinrpc_method_set_nonstandard(bitcoinrpc_method_t *method, char *name);

/* ------------- bitcoinrpc_resp --------------------- */
struct bitcoinrpc_resp;

typedef
struct bitcoinrpc_resp
bitcoinrpc_resp_t;

bitcoinrpc_resp_t *
bitcoinrpc_resp_init(void);

BITCOINRPCEcode
bitcoinrpc_resp_free(bitcoinrpc_resp_t *resp);

/*
   Get a deepcopy of the json object representing the response
   from the server or NULL in case of error.
 */
json_t *
bitcoinrpc_resp_get(bitcoinrpc_resp_t *resp);

/*
   Check if the resp comes as a result of calling method.
   Returns BITCOINRPCE_CHECK, if not. This check is already performed by
   bitcoinrpc_call()
 */
BITCOINRPCEcode
bitcoinrpc_resp_check(bitcoinrpc_resp_t *resp, bitcoinrpc_method_t *method);


/* ------------- bitcoinrpc_call --------------------- */

/*
   Call the server with method. Save response in resp.
   If e == NULL, it is ignored.
 */
BITCOINRPCEcode
bitcoinrpc_call(bitcoinrpc_cl_t *cl, bitcoinrpc_method_t * method,
                bitcoinrpc_resp_t *resp, bitcoinrpc_err_t *e);


/*
   Call the server with a contingent array of pointers to methods
   (JSON_RPC method batching) where n is the length of the array. Save the
   response in the contingent array resps of pointers to response objects
   (also of the length n). Save error messages in e. If e == NULL, it
   is ignored. If n == 1, it is the same as bitcoinrpc_call().
 */
BITCOINRPCEcode
bitcoinrpc_calln(bitcoinrpc_cl_t * cl, size_t n, bitcoinrpc_method_t **methods,
                 bitcoinrpc_resp_t **resps, bitcoinrpc_err_t *e);


#endif /* BITCOINRPC_H_51fe7847_aafe_4e78_9823_eff094a30775 */
