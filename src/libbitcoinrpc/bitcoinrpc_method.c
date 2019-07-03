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

#include <string.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include "bitcoinrpc.h"
#include "bitcoinrpc_global.h"
#include "bitcoinrpc_method.h"

/*
   Names of Bitcoin RPC methods and how to get them from BITCOINRPC_METHOD codes.
 */
struct BITCOINRPC_METHOD_struct_ {
  BITCOINRPC_METHOD m;
  char *str;
};

#define BITCOINRPC_METHOD_names_len_ 83   /* remember to update it! */

const struct BITCOINRPC_METHOD_struct_
  BITCOINRPC_METHOD_names_[BITCOINRPC_METHOD_names_len_] =
{
  { BITCOINRPC_METHOD_NONSTANDARD, "" },

  /* Blockchain RPCs */
  { BITCOINRPC_METHOD_GETBESTBLOCKHASH, "getbestblockhash" },
  { BITCOINRPC_METHOD_GETBLOCK, "getblock" },
  { BITCOINRPC_METHOD_GETBLOCKCHAININFO, "getblockchaininfo" },
  { BITCOINRPC_METHOD_GETBLOCKCOUNT, "getblockcount" },
  { BITCOINRPC_METHOD_GETBLOCKHASH, "getblockhash" },
  { BITCOINRPC_METHOD_GETCHAINTIPS, "getchaintips" },
  { BITCOINRPC_METHOD_GETDIFFICULTY, "getdifficulty" },
  { BITCOINRPC_METHOD_GETMEMPOOLINFO, "getmempoolinfo" },
  { BITCOINRPC_METHOD_GETRAWMEMPOOL, "getrawmempool" },
  { BITCOINRPC_METHOD_GETTXOUT, "gettxout" },
  { BITCOINRPC_METHOD_GETTXOUTPROOF, "gettxoutproof" },
  { BITCOINRPC_METHOD_GETTXOUTSETINFO, "gettxoutsetinfo" },
  { BITCOINRPC_METHOD_VERIFYCHAIN, "verifychain" },
  { BITCOINRPC_METHOD_VERIFYTXOUTPROOF, "verifytxoutproof" },

  /* Control RPCs */
  { BITCOINRPC_METHOD_GETINFO, "getinfo" },
  { BITCOINRPC_METHOD_HELP, "help" },
  { BITCOINRPC_METHOD_STOP, "stop" },

  /* Generating RPCs */
  { BITCOINRPC_METHOD_GENERATE, "generate" },
  { BITCOINRPC_METHOD_GETGENERATE, "getgenerate" },
  { BITCOINRPC_METHOD_SETGENERATE, "setgenerate" },

  /* Mining RPCs */
  { BITCOINRPC_METHOD_GETBLOCKTEMPLATE, "getblocktemplate" },
  { BITCOINRPC_METHOD_GETMININGINFO, "getmininginfo" },
  { BITCOINRPC_METHOD_GETNETWORKHASHPS, "getnetworkhashps" },
  { BITCOINRPC_METHOD_PRIORITISETRANSACTION, "prioritisetransaction" },
  { BITCOINRPC_METHOD_SUBMITBLOCK, "submitblock" },

  /* Network RPCs */
  { BITCOINRPC_METHOD_ADDNODE, "addnode" },
  { BITCOINRPC_METHOD_GETADDEDNODEINFO, "getaddednodeinfo" },
  { BITCOINRPC_METHOD_GETCONNECTIONCOUNT, "getconnectioncount" },
  { BITCOINRPC_METHOD_GETNETTOTALS, "getnettotals" },
  { BITCOINRPC_METHOD_GETNETWORKINFO, "getnetworkinfo" },
  { BITCOINRPC_METHOD_GETPEERINFO, "getpeerinfo" },
  { BITCOINRPC_METHOD_PING, "ping" },

  /* Raw transaction RPCs */
  { BITCOINRPC_METHOD_CREATERAWTRANSACTION, "createrawtransaction" },
  { BITCOINRPC_METHOD_DECODERAWTRANSACTION, "decoderawtransaction" },
  { BITCOINRPC_METHOD_DECODESCRIPT, "decodescript" },
  { BITCOINRPC_METHOD_GETRAWTRANSACTION, "getrawtransaction" },
  { BITCOINRPC_METHOD_SENDRAWTRANSACTION, "sendrawtransaction" },
  { BITCOINRPC_METHOD_SIGNRAWTRANSACTION, "signrawtransaction" },

  /* Utility RPCs */
  { BITCOINRPC_METHOD_CREATEMULTISIG, "createmultisig" },
  { BITCOINRPC_METHOD_ESTIMATEFEE, "estimatefee" },
  { BITCOINRPC_METHOD_ESTIMATEPRIORITY, "estimatepriority" },
  { BITCOINRPC_METHOD_VALIDATEADDRESS, "validateaddress" },
  { BITCOINRPC_METHOD_VERIFYMESSAGE, "verifymessage" },

  /* Wallet RPCs */
  { BITCOINRPC_METHOD_ADDMULTISIGADDRESS, "addmultisigaddress" },
  { BITCOINRPC_METHOD_BACKUPWALLET, "backupwallet" },
  { BITCOINRPC_METHOD_DUMPPRIVKEY, "dumpprivkey" },
  { BITCOINRPC_METHOD_DUMPWALLET, "dumpwallet" },
  { BITCOINRPC_METHOD_ENCRYPTWALLET, "encryptwallet" },
  { BITCOINRPC_METHOD_GETACCOUNTADDRESS, "getaccountaddress" },
  { BITCOINRPC_METHOD_GETACCOUNT, "getaccount" },
  { BITCOINRPC_METHOD_GETADDRESSESBYACCOUNT, "getaddressesbyaccount" },
  { BITCOINRPC_METHOD_GETBALANCE, "getbalance" },
  { BITCOINRPC_METHOD_GETNEWADDRESS, "getnewaddress" },
  { BITCOINRPC_METHOD_GETRAWCHANGEADDRESS, "getrawchangeaddress" },
  { BITCOINRPC_METHOD_GETRECEIVEDBYACCOUNT, "getreceivedbyaccount" },
  { BITCOINRPC_METHOD_GETRECEIVEDBYADDRESS, "getreceivedbyaddress" },
  { BITCOINRPC_METHOD_GETTRANSACTION, "gettransaction" },
  { BITCOINRPC_METHOD_GETUNCONFIRMEDBALANCE, "getunconfirmedbalance" },
  { BITCOINRPC_METHOD_GETWALLETINFO, "getwalletinfo" },
  { BITCOINRPC_METHOD_IMPORTADDRESS, "importaddress" },
  { BITCOINRPC_METHOD_IMPORTPRIVKEY, "importprivkey" },
  { BITCOINRPC_METHOD_IMPORTWALLET, "importwallet" },
  { BITCOINRPC_METHOD_KEYPOOLREFILL, "keypoolrefill" },
  { BITCOINRPC_METHOD_LISTACCOUNTS, "listaccounts" },
  { BITCOINRPC_METHOD_LISTADDRESSGROUPINGS, "listaddressgroupings" },
  { BITCOINRPC_METHOD_LISTLOCKUNSPENT, "listlockunspent" },
  { BITCOINRPC_METHOD_LISTRECEIVEDBYACCOUNT, "listreceivedbyaccount" },
  { BITCOINRPC_METHOD_LISTRECEIVEDBYADDRESS, "listreceivedbyaddress" },
  { BITCOINRPC_METHOD_LISTSINCEBLOCK, "listsinceblock" },
  { BITCOINRPC_METHOD_LISTTRANSACTIONS, "listtransactions" },
  { BITCOINRPC_METHOD_LISTUNSPENT, "listunspent" },
  { BITCOINRPC_METHOD_LOCKUNSPENT, "lockunspent" },
  { BITCOINRPC_METHOD_MOVE, "move" },
  { BITCOINRPC_METHOD_SENDFROM, "sendfrom" },
  { BITCOINRPC_METHOD_SENDMANY, "sendmany" },
  { BITCOINRPC_METHOD_SENDTOADDRESS, "sendtoaddress" },
  { BITCOINRPC_METHOD_SETACCOUNT, "setaccount" },
  { BITCOINRPC_METHOD_SETTXFEE, "settxfee" },
  { BITCOINRPC_METHOD_SIGNMESSAGE, "signmessage" },
  { BITCOINRPC_METHOD_WALLETLOCK, "walletlock" },
  { BITCOINRPC_METHOD_WALLETPASSPHRASE, "walletpassphrase" },
  { BITCOINRPC_METHOD_WALLETPASSPHRASECHANGE, "walletpassphrasechange" }
};


static const struct BITCOINRPC_METHOD_struct_ *
bitcoinrpc_method_st_(const BITCOINRPC_METHOD m)
{
  for (int i = 0; i < BITCOINRPC_METHOD_names_len_; i++)
    {
      if (BITCOINRPC_METHOD_names_[i].m == m)
        return &BITCOINRPC_METHOD_names_[i];
    }
  return NULL;
}
/* ------------------------------------------------------------------------- */




/*
   Internal methods
 */

static BITCOINRPCEcode
bitcoinrpc_method_make_postjson_(bitcoinrpc_method_t *method)
{
  if (NULL == method)
    return BITCOINRPCE_BUG;

  if (NULL != method->post_json)
    {
      json_decref(method->post_json);
      method->post_json = json_object();
      if (NULL == method->post_json)
        return BITCOINRPCE_JSON;
    }

  json_object_set_new(method->post_json, "method", json_string(method->mstr));
  json_object_set_new(method->post_json, "id", json_string(method->uuid_str));

  if (NULL == method->params_json)
    {
      json_object_set_new(method->post_json, "params", json_array());
    }
  else
    {
      json_object_set(method->post_json, "params", method->params_json);
    }

  return BITCOINRPCE_OK;
}


json_t *
bitcoinrpc_method_get_postjson_(bitcoinrpc_method_t *method)
{
  if (NULL == method)
    return NULL;

  return method->post_json;
}


BITCOINRPCEcode
bitcoinrpc_method_compare_uuid_(bitcoinrpc_method_t *method, uuid_t u)
{
  if (NULL == method)
    return BITCOINRPCE_BUG;

  return (uuid_compare(method->uuid, u) == 0) ?
         BITCOINRPCE_OK : BITCOINRPCE_CHECK;
}


static BITCOINRPCEcode
bitcoinrpc_method_update_uuid_(bitcoinrpc_method_t *method)
{
  if (NULL == method)
    return BITCOINRPCE_BUG;

  uuid_generate_random(method->uuid);
  uuid_unparse_lower(method->uuid, method->uuid_str);

  return bitcoinrpc_method_make_postjson_(method);
}


/* ------------------------------------------------------------------------  */

bitcoinrpc_method_t *
bitcoinrpc_method_init(const BITCOINRPC_METHOD m)
{
  return bitcoinrpc_method_init_params(m, NULL);
}


bitcoinrpc_method_t *
bitcoinrpc_method_init_params(const BITCOINRPC_METHOD m,
                              json_t * const params)
{
  json_t *jp = NULL;

  if (NULL == params)
    {
      jp = NULL;
    }
  else
    {
      jp = json_deep_copy(params);
      if (NULL == jp)
        return NULL;
    }

  bitcoinrpc_method_t *method = bitcoinrpc_global_allocfunc(sizeof *method);

  if (NULL == method)
    {
      if (jp != NULL)
        json_decref(jp);
      return NULL;
    }

  method->m = m;
  const struct BITCOINRPC_METHOD_struct_ * ms = bitcoinrpc_method_st_(method->m);
  method->mstr = ms->str;
  method->params_json = jp;

  /* make post_json */
  method->post_json = NULL;
  method->post_json = json_object();
  if (NULL == method->post_json || bitcoinrpc_method_update_uuid_(method) != BITCOINRPCE_OK)
    {
      if (jp != NULL)
        json_decref(jp);
      bitcoinrpc_global_freefunc(method);
      return NULL;
    }

  return method;
}


BITCOINRPCEcode
bitcoinrpc_method_free(bitcoinrpc_method_t *method)
{
  if (NULL == method)
    return BITCOINRPCE_ARG;

  json_decref(method->post_json);
  if (method->params_json != NULL)
    json_decref(method->params_json);

  bitcoinrpc_global_freefunc(method);
  method = NULL;

  return BITCOINRPCE_OK;
}


/* Set a new json object as method parameters */
BITCOINRPCEcode
bitcoinrpc_method_set_params(bitcoinrpc_method_t *method, json_t *params)
{
  json_t *jp = NULL;

  if (NULL == method)
    return BITCOINRPCE_ARG;

  if (NULL == params)
    {
      jp = NULL;
    }
  else
    {
      jp = json_deep_copy(params);
      if (NULL == jp)
        return BITCOINRPCE_JSON;
    }

  if (method->params_json != NULL)
    json_decref(method->params_json);

  method->params_json = jp;

  return bitcoinrpc_method_update_uuid_(method);
}


BITCOINRPCEcode
bitcoinrpc_method_get_params(bitcoinrpc_method_t *method, json_t **params)
{
  json_t *jp = NULL;

  if (NULL == params || NULL == method)
    return BITCOINRPCE_ARG;

  if (NULL == method->params_json)
    {
      jp = NULL;
    }
  else
    {
      jp = json_deep_copy(method->params_json);
      if (NULL == jp)
        return BITCOINRPCE_JSON;
    }

  *params = jp;

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_method_set_nonstandard(bitcoinrpc_method_t *method, char *name)
{
  if (NULL == method)
    return BITCOINRPCE_ARG;

  if (method->m != BITCOINRPC_METHOD_NONSTANDARD)
    return BITCOINRPCE_ERR;

  method->mstr = name;

  return bitcoinrpc_method_update_uuid_(method);
}


char *
bitcoinrpc_method_get_mstr_(bitcoinrpc_method_t *method)
{
  if (method == NULL)
    return NULL;

  return method->mstr;
}
