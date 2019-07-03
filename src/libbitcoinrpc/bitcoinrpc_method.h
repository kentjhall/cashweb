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
   Access bitcoinrpc_method_t internals
 */

#ifndef BITCOINRPC_METHOD_H_1d9cedfd_a1d6_4b80_9ad4_fcc4549abcad
#define BITCOINRPC_METHOD_H_1d9cedfd_a1d6_4b80_9ad4_fcc4549abcad

#include <uuid/uuid.h>
#include "bitcoinrpc.h"


struct bitcoinrpc_method {
  BITCOINRPC_METHOD m;
  char* mstr;

  uuid_t uuid;
  char uuid_str[37];      /* why 37? see: man 3 uuid_unparse */

  json_t  *params_json;
  json_t  *post_json;

  /*
     This is a legacy pointer. You can point to an auxilliary structure,
     if you prefer not to touch this one (e.g. not to break ABI).
   */
  void *legacy_ptr_025ed4e5_7a59_4086_83b5_abc3a4767894;
};


BITCOINRPCEcode
bitcoinrpc_method_compare_uuid_(bitcoinrpc_method_t *method, uuid_t u);

json_t *
bitcoinrpc_method_get_postjson_(bitcoinrpc_method_t *method);

char *
bitcoinrpc_method_get_mstr_(bitcoinrpc_method_t *method);

#endif /* BITCOINRPC_METHOD_H_1d9cedfd_a1d6_4b80_9ad4_fcc4549abcad */
