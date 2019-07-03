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

/*
   Access _resp internals
 */

#ifndef BITCOINRPC_RESP_H_fba55207_d817_4c13_b509_5ac187863cb9
#define BITCOINRPC_RESP_H_fba55207_d817_4c13_b509_5ac187863cb9

#include <uuid/uuid.h>
#include "bitcoinrpc.h"


struct bitcoinrpc_resp {
  uuid_t uuid;
  json_t  *json;

  /*
     This is a legacy pointer. You can point to an auxilliary structure,
     if you prefer not to touch this one (e.g. not to break ABI).
   */
  void* legacy_ptr_0a942f71_58af_4869_a9f8_4a7c48ddae9c;
};


BITCOINRPCEcode
bitcoinrpc_resp_set_json_(bitcoinrpc_resp_t *resp, json_t *json);

#endif /* BITCOINRPC_RESP_H_fba55207_d817_4c13_b509_5ac187863cb9 */
