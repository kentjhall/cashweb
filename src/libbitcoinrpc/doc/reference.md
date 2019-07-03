# bitcoinrpc Reference

The reference consists of two parts: ABI and API.


## ABI

Much from the libcurl ABI description applies here too,
see [libcurl ABI](http://curl.haxx.se/libcurl/abi.html).

ABI stands for *Application Binary Interface* and it
*determines such details as how functions are called and in which binary format
information should be passed from one program component to the next*
([Wikipedia has it all](https://en.wikipedia.org/wiki/Application_binary_interface)).

### Upgrades

The code is currently in beta stage and some updates could break ABI.
You are advised to follow the development and upgrade your code from time to
time until the project goes into a stable phase.

### Version number

The version numbers of the library follow
[Semantic versioning](http://semver.org/#semantic-versioning-200)
scheme, i.e. each version consists of three numbers: MAJOR.MINOR.PATCH.
The major version 0 denotes beta stage. *Except for major version 0*, only
major version updates could (and usually do) break backwards compatibility.
In the beta stage also minor version updates could break ABI, whereas in the
usual case they are reserved for extensions and major bug fixes.
See `Changelog.md` for the detailed history and changes introduced by each
version.

### Soname bumps

Whenever there are changes done to the library that will cause an ABI breakage,
a soname major number of the library is bumped to a higher one. Again,
**except for beta**.  In fact, the soname number and the major version number
should stay the same in the future.
The history of soname bumps looks as follows:

  * libbitcoinrpc.so.0, *January 2016*


## API

The library API consists of five segments:

* `bitcoinrpc_global`
* `bitcoinrpc_cl`
* `bitcoinrpc_method`
* `bitcoinrpc_resp`
* `bitcoinrpc_err`

Additionally, there is one function called `bitcoinrpc_call()`.
See [design](./design.md) document for a general overview.

The whole user interface is contained in the main header, so to use the
library it is enough to

    #include <bitcoinrpc.h>

What follows is a detailed description of each method and data structure
within the library.  For a quick and easy introduction to typical use cases,
see [tutorial](./tutorial.md) and [examples](./examples.md).


### Defined constants

The following is the list of preprocessor constants defined in
the `bitcoinrpc.h` header.

* `BITCOINRPC_LIBNAME` = "bitcoinrpc"


* `BITCOINRPC_VERSION`

  Version string: "BITCOINRPC_VERSION_MAJOR.BITCOINRPC_VERSION_MINOR.BITCOINRPC_VERSION_PATCH"
  like: "0.1.0" or "2.31.11"


* `BITCOINTPC_VERSION_MAJOR`
* `BITCOINTPC_VERSION_MINOR`
* `BITCOINTPC_VERSION_PATCH`

  Integers specifying the major, minor version number and patch, respectively.


* `BITCOINRPC_VERSION_HEX`

    A 3-byte hexadecimal representation of the version, e.g. 0x000100
    for version 0.1.0 and 0x010300 for version 1.3.
    This is useful in numeric comparisons, e.g.:

```

    #if BITCOIN_VERSION_HEX >= 0x020100
    /* Code specific to version 2.1 and above */
    #endif
```


* `BITCOINRPC_USER_DEFAULT` = ""
* `BITCOINRPC_PASS_DEFAULT` = ""
* `BITCOINRPC_ADDR_DEFAULT` = "127.0.0.1"
* `BITCOINRPC_PORT_DEFAULT` = 8332

  Defalut parameters of a RPC client.


* `BITCOINRPC_PARAM_MAXLEN` = 65 (64 bytes + `'\0'`)

  Maximal length of a string that holds a client's parameter
  (user name, password or address), including the terminating `'\0'` character.
  The default value: 64 bytes, holds any SHA256 hash, so it is more than enough.


* `BITCOINRPC_URL_MAXLEN`

  Maximal length of the server url:
  `"http://%s:%d" = 2*BITCOINRPC_PARAM_MAXLEN + 13`


* `BITCOINRPC_ERRMSG_MAXLEN`

  Maximal length of error message reported via `bitcoinrpc_err_t`


### bitcoinrpc_satoshi_t

To handle bitcoin amounts correctly, it is advisable to operate internally
only on satoshi units (one hunder millionth of a bitcoin), and avoid floating
point arithmetic completely.  Therefore, many functions in this library
take an argument that is of the type: `bitcoinrpc_satoshi_t`, defined as

```
    typedef unsigned long long int bitcoinrpc_satoshi_t;
```

Please, see:
[Proper Money Handling](https://en.bitcoin.it/wiki/Proper_Money_Handling_\(JSON-RPC\))
article at Bitcoin wiki.



### Error codes and bitcoinrpc_err

The error codes are defined as enum type `BITCOINRPCEcode`:

```

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
```

The data structure handling custom error messages has typedef:
`bitcoinrpc_err_t` and its internals are visible to the user:


```

    struct bitcoinrpc_err
    {
      BITCOINRPCEcode code;
      char msg[BITCOINRPC_ERRMSG_MAXLEN];
    };

    typedef
      struct bitcoinrpc_err
    bitcoinrpc_err_t;
```

If `code == BITCOINRPCE_OK` after calling a function that takes pointer to
`bitcoinrpc_err_t` as an argument, then the error message `msg` is irrelevant
(usually set to `NULL`).  On the other hand, if `code != BITCOINRPCE_OK`,
then `msg` contains some custom error message of length at most
`BITCOINRPC_ERRMSG_MAXLEN`.  If `code == BITCOINRPCE_SERV` then the error
message is a string containing the error message returned by the server,
i.e. the call was successful, but the server could not process the call,
(e.g. due to wrong parameters or the wallet being locked).


### bitcoinrpc_global

Routines to initialise the global state of the library and to clean up the
library's internal garbage at the end of the program.


* `BITCOINRPCEcode`
  **bitcoinrpc_global_init** `(void)`

  The global initialisation function.
  Please call this function from your main thread before any other call. <br>
  *Return*: `BITCOINRPCE_OK` in case of success.


* `BITCOINRPCEcode`
  **bitcoinrpc_global_cleanup** `(void)`

  The global cleanup function.
  Please call this function at the end of your program to collect
  library's internal garbage. <br>
  *Return*: `BITCOINRPCE_OK`.


* `BITCOINRPCEcode`
  **bitcoinrpc_global_set_allocfunc** `( void* (* const f) (size_t size) )`

  Set a memory allocating function for the library routines
  (the default is just standard malloc). If `f == NULL` he function does nothing.<br>
  *Return*: `BITCOINRPCE_OK` or `BITCOINRPCE_ARG`, if `f == NULL`.


* `BITCOINRPCEcode`
  **bitcoinrpc_global_set_freefunc** `( void (* const f) (void *ptr) )`

  Set a memory freeing function for the library routines
  (the default is just standard free). If `f == NULL` he function does nothing.<br>
  *Return*: `BITCOINRPCE_OK` or `BITCOINRPCE_ARG`, if `f == NULL`.


### bitcoinrpc_cl

Routines to handle RPC client.

* **bitcoinrpc_cl_t**

  Type definition of the RPC client data structure.


* `bitcoinrpc_cl_t*`
  **bitcoinrpc_cl_init** `(void)`

  Initialise a new RPC client with default values:
  `BITCOINRPC_USER_DEFAULT`, `BITCOINRPC_PASS_DEFAULT` etc. <br>
  *Return*: a newly allocated handle or NULL in case of error.


* `bitcoinrpc_cl_t*`
  **bitcoinrpc_cl_init_params**
      `(const char *user, const char *pass,
        const char *addr, const unsigned int port )`

  Initialise a new RPC client and set some parameters
  (may not be `NULL`; in that case the function returns `NULL` as well).
  The parameter values are copied, so the original pointers are no longer
  needed. At most `BITCOINRPC_PARAM_MAXLEN` chars are copied to store
  a parameter. <br>
  *Return*: a newly allocated handle or NULL in case of error.


* `BITCOINRPCEcode`
  **bitcoinrpc_cl_free** `(bitcoinrpc_cl_t *cl)`

  Free the handle. The pointer `cl` is set to `NULL` after the function call.<br>
  *Return*: `BITCOINRPCE_OK`.


* `BITCOINRPCEcode`
  **bitcoinrpc_cl_get_user** `(bitcoinrpc_cl_t *cl, char *buf)`

* `BITCOINRPCEcode`
  **bitcoinrpc_cl_get_pass** `(bitcoinrpc_cl_t *cl, char *buf)`

* `BITCOINRPCEcode`
  **bitcoinrpc_cl_get_addr** `(bitcoinrpc_cl_t *cl, char *buf)`

* `BITCOINRPCEcode`
  **bitcoinrpc_cl_get_port** `(bitcoinrpc_cl_t *cl, unsigned int *bufi)`

  Copy a value of the client `cl` parameter to `buf`. The buffer is assumed
  to contain at least `BITCOINRPC_PARAM_MAXLEN` chars.
  At most `BITCOINRPC_PARAM_MAXLEN` chars are copied. <br>
  *Return*: `BITCOINRPCE_OK` or `BITCOINRPCE_ARG` in case of wrong arguments.


* `BITCOINRPCEcode`
  **bitcoinrpc_cl_get_url** `(bitcoinrpc_cl_t \*cl, char \*buf)`

  Copy value to `buf`. The buffer is assumed to contain at least
  `BITCOINRPC_URL_MAXLEN` chars.
  At most `BITCOINRPC_URL_MAXLEN` chars are copied. <br>
  *Return*: `BITCOINRPCE_OK` or `BITCOINRPCE_ARG` in case of wrong arguments.


### bitcoinrpc_method

Routines to handle an RPC method.

The argument `json_t *params` to functions below is always copied
to library internals and the original pointer is no longer needed
after the function returns.  It is the obligation of the user to free
the original pointer by decreasing its reference count:
[`json_decref(params)`](https://jansson.readthedocs.org/en/2.7/apiref.html#c.json_decref).


* **bitcoinrpc_method_t**

  Type definition of a method object.


* **BITCOINRPC_METHOD**

  The enum type storing RPC method names.
  See Bitcoin [RPC reference](https://bitcoin.org/en/developer-reference#rpc-quick-reference).

```

    typedef enum {

    BITCOINRPC_METHOD_NONSTANDARD,           /* reserved for possible */
                                             /* user extensions       */

    BITCOINRPC_METHOD_GETBESTBLOCKHASH,      /* getbestblockhash */
    BITCOINRPC_METHOD_GETBLOCK,              /* getblock */
    BITCOINRPC_METHOD_GETBLOCKCHAININFO,     /* getblockchaininfo */
    BITCOINRPC_METHOD_GETBLOCKCOUNT,         /* getblockcount */
    BITCOINRPC_METHOD_GETBLOCKHASH,          /* getblockhash */
    BITCOINRPC_METHOD_GETCHAINTIPS,          /* getchaintips */
    BITCOINRPC_METHOD_GETDIFFICULTY,         /* getdifficulty */
    BITCOINRPC_METHOD_GETMEMPOOLINFO,        /* getmempoolinfo */
    BITCOINRPC_METHOD_GETRAWMEMPOOL,         /* getrawmempool */
    BITCOINRPC_METHOD_GETTXOUT,              /* gettxout */
    BITCOINRPC_METHOD_GETTXOUTPROOF,         /* gettxoutproof */
    BITCOINRPC_METHOD_GETTXOUTSETINFO,       /* gettxoutsetinfo */
    BITCOINRPC_METHOD_VERIFYCHAIN,           /* verifychain */
    BITCOINRPC_METHOD_VERIFYTXOUTPROOF,      /* verifytxoutproof */

      /* Control RPCs */
    BITCOINRPC_METHOD_GETINFO,               /* getinfo */
    BITCOINRPC_METHOD_HELP,                  /* help */
    BITCOINRPC_METHOD_STOP,                  /* stop */

      /* Generating RPCs */
    BITCOINRPC_METHOD_GENERATE,              /* generate */
    BITCOINRPC_METHOD_GETGENERATE,           /* getgenerate */
    BITCOINRPC_METHOD_SETGENERATE,           /* setgenerate */

      /* Mining RPCs */
    BITCOINRPC_METHOD_GETBLOCKTEMPLATE,      /* getblocktemplate */
    BITCOINRPC_METHOD_GETMININGINFO,         /* getmininginfo */
    BITCOINRPC_METHOD_GETNETWORKHASHPS,      /* getnetworkhashps */
    BITCOINRPC_METHOD_PRIORITISETRANSACTION, /* prioritisetransaction */
    BITCOINRPC_METHOD_SUBMITBLOCK,           /* submitblock */

      /* Network RPCs */
    BITCOINRPC_METHOD_ADDNODE,               /* addnode */
    BITCOINRPC_METHOD_GETADDEDNODEINFO,      /* getaddednodeinfo */
    BITCOINRPC_METHOD_GETCONNECTIONCOUNT,    /* getconnectioncount */
    BITCOINRPC_METHOD_GETNETTOTALS,          /* getnettotals */
    BITCOINRPC_METHOD_GETNETWORKINFO,        /* getnetworkinfo */
    BITCOINRPC_METHOD_GETPEERINFO,           /* getpeerinfo */
    BITCOINRPC_METHOD_PING,                  /* ping */

      /* Raw transaction RPCs */
    BITCOINRPC_METHOD_CREATERAWTRANSACTION,  /* createrawtransaction */
    BITCOINRPC_METHOD_DECODERAWTRANSACTION,  /* decoderawtransaction */
    BITCOINRPC_METHOD_DECODESCRIPT,          /* decodescript */
    BITCOINRPC_METHOD_GETRAWTRANSACTION,     /* getrawtransaction */
    BITCOINRPC_METHOD_SENDRAWTRANSACTION,    /* sendrawtransaction */
    BITCOINRPC_METHOD_SIGNRAWTRANSACTION,    /* signrawtransaction */

      /* Utility RPCs */
    BITCOINRPC_METHOD_CREATEMULTISIG,        /* createmultisig */
    BITCOINRPC_METHOD_ESTIMATEFEE,           /* estimatefee */
    BITCOINRPC_METHOD_ESTIMATEPRIORITY,      /* estimatepriority */
    BITCOINRPC_METHOD_VALIDATEADDRESS,       /* validateaddress */
    BITCOINRPC_METHOD_VERIFYMESSAGE,         /* verifymessage */

      /* Wallet RPCs */
    BITCOINRPC_METHOD_ADDMULTISIGADDRESS,    /* addmultisigaddress */
    BITCOINRPC_METHOD_BACKUPWALLET,          /* backupwallet */
    BITCOINRPC_METHOD_DUMPPRIVKEY,           /* dumpprivkey */
    BITCOINRPC_METHOD_DUMPWALLET,            /* dumpwallet */
    BITCOINRPC_METHOD_ENCRYPTWALLET,         /* encryptwallet */
    BITCOINRPC_METHOD_GETACCOUNTADDRESS,     /* getaccountaddress */
    BITCOINRPC_METHOD_GETACCOUNT,            /* getaccount */
    BITCOINRPC_METHOD_GETADDRESSESBYACCOUNT, /* getaddressesbyaccount */
    BITCOINRPC_METHOD_GETBALANCE,            /* getbalance */
    BITCOINRPC_METHOD_GETNEWADDRESS,         /* getnewaddress */
    BITCOINRPC_METHOD_GETRAWCHANGEADDRESS,   /* getrawchangeaddress */
    BITCOINRPC_METHOD_GETRECEIVEDBYACCOUNT,  /* getreceivedbyaccount */
    BITCOINRPC_METHOD_GETRECEIVEDBYADDRESS,  /* getreceivedbyaddress */
    BITCOINRPC_METHOD_GETTRANSACTION,        /* gettransaction */
    BITCOINRPC_METHOD_GETUNCONFIRMEDBALANCE, /* getunconfirmedbalance */
    BITCOINRPC_METHOD_GETWALLETINFO,         /* getwalletinfo */
    BITCOINRPC_METHOD_IMPORTADDRESS,         /* importaddress */
    BITCOINRPC_METHOD_IMPORTPRIVKEY,         /* importprivkey */
    BITCOINRPC_METHOD_IMPORTWALLET,          /* importwallet */
    BITCOINRPC_METHOD_KEYPOOLREFILL,         /* keypoolrefill */
    BITCOINRPC_METHOD_LISTACCOUNTS,          /* listaccounts */
    BITCOINRPC_METHOD_LISTADDRESSGROUPINGS,  /* listaddressgroupings */
    BITCOINRPC_METHOD_LISTLOCKUNSPENT,       /* listlockunspent */
    BITCOINRPC_METHOD_LISTRECEIVEDBYACCOUNT, /* listreceivedbyaccount */
    BITCOINRPC_METHOD_LISTRECEIVEDBYADDRESS, /* listreceivedbyaddress */
    BITCOINRPC_METHOD_LISTSINCEBLOCK,        /* listsinceblock */
    BITCOINRPC_METHOD_LISTTRANSACTIONS,      /* listtransactions */
    BITCOINRPC_METHOD_LISTUNSPENT,           /* listunspent */
    BITCOINRPC_METHOD_LOCKUNSPENT,           /* lockunspent */
    BITCOINRPC_METHOD_MOVE,                  /* move */
    BITCOINRPC_METHOD_SENDFROM,              /* sendfrom */
    BITCOINRPC_METHOD_SENDMANY,              /* sendmany */
    BITCOINRPC_METHOD_SENDTOADDRESS,         /* sendtoaddress */
    BITCOINRPC_METHOD_SETACCOUNT,            /* setaccount */
    BITCOINRPC_METHOD_SETTXFEE,              /* settxfee */
    BITCOINRPC_METHOD_SIGNMESSAGE,           /* signmessage */
    BITCOINRPC_METHOD_WALLETLOCK,            /* walletlock */
    BITCOINRPC_METHOD_WALLETPASSPHRASE,      /* walletpassphrase */
    BITCOINRPC_METHOD_WALLETPASSPHRASECHANGE /* walletpassphrasechange */

    } BITCOINRPC_METHOD;

```


* `bitcoinrpc_method_t *`
  **bitcoinrpc_method_init** `(const BITCOINRPC_METHOD m)`

  Initialise a new bare method. <br>
  *Return*: a newly allocated method or `NULL` in case of error.


* `bitcoinrpc_method_t *`
  **bitcoinrpc_method_init_params**
      `(const BITCOINRPC_METHOD m, json_t * const params)`

  Initialise a new method with `json_t` array: `params`.
  If `params == NULL`, this is the same as `bitcoinrpc_method_init()`. <br>
  *Return*: a newly allocated method or `NULL` in case of error.


* `BITCOINRPCEcode`
  **bitcoinrpc_method_free** `(bitcoinrpc_method_t *method)`

  Destroy the method and set the pointer `method` to `NULL`.
  *Return*: `BITCOINRPCE_OK`.


* `BITCOINRPCEcode`
  **bitcoinrpc_method_set_params**
      `(bitcoinrpc_method_t *method, json_t *params)`

  Set a new `json_t` object as method parameters. <br>
  *Return*: `BITCOINRPCE_OK`, or `BITCOINRPCE_JSON` if `params` cannot
  be parsed.


* `BITCOINRPCEcode`
  **bitcoinrpc_method_get_params**
      `(bitcoinrpc_method_t *method, json_t **params)`

  Get a deep copy of the method's parameters and store it in `params`. <br>
  *Return*: `BITCOINRPCE_OK`, or `BITCOINRPCE_JSON` if `params` cannot
  be copied by libjansson.


* `BITCOINRPCEcode`
  **bitcoinrpc_method_setname** `(bitcoinrpc_method_t *method, char *name)`

  Set a custom name for the method.
  Works only if method is initialised with BITCOINRPC_METHOD_NONSTANDARD,
  otherwise returns BITCOINRPCE_ERR.
  The pointer to str is copied, not the string itself. It is the obligation of
  the user, to keep the str available and free it, when no longer needed.
  *Return*: `BITCOINRPCE_OK` or `BITCOINRPCE_ERR`.


### bitcoinrpc_resp

Store JSON responses from the server.


* **bitcoinrpc_resp_t**

  Type definition of the response struct.


* `bitcoinrpc_resp_t *`
  **bitcoinrpc_resp_init** `(void)`

  Initialise a new bare response. <br>
  *Return*: a newly allocated response or `NULL` in case of error.


* `BITCOINRPCEcode`
  **bitcoinrpc_resp_free** `(bitcoinrpc_resp_t *resp)`

  Destroy the response `resp` and set the pointer to `NULL`. <br>
  *Return*: `BITCOINRPCE_OK`.


* `json_t *`
  **bitcoinrpc_resp_get** `(bitcoinrpc_resp_t *resp)`

  Get a deep copy of the JSON object representing the response from the server. <br>
  *Return*: a newly allocated `json_t` object or `NULL` in case of error.


* `BITCOINRPCEcode`
  **bitcoinrpc_resp_check**
      `(bitcoinrpc_resp_t *resp, bitcoinrpc_method_t *method)`

  Check, if the `resp` comes as a result of calling `method`.
  This check is already performed by `bitcoinrpc_call()`. <br>
  *Returns*: `BITCOINRPCE_OK` or `BITCOINRPCE_CHECK`, if check fails.


### bitcoinrpc_call()

* `BITCOINRPCEcode`
  **bitcoinrpc_call**
      `(bitcoinrpc_cl_t * cl, bitcoinrpc_method_t * method,
                 bitcoinrpc_resp_t *resp, bitcoinrpc_err_t *e)`

 Use client `cl` to call the server with `method`. Save response in `resp`
 and report errors. If `e == NULL`, it is ignored. <br>
 *Return*: `BITCOINRPCE_OK` in case of success, or other error code.

*last updated: 2016-02-06*
