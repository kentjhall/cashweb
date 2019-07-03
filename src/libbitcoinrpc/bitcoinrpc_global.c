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

#include <stdlib.h>
#include <curl/curl.h>
#include "bitcoinrpc.h"
#include "bitcoinrpc_global.h"

/* so far useless */
struct bitcoinrpc_global_data_s_ {
  char* str;

  /*
     This is a legacy pointer. You can point to an auxilliary structure,
     if you prefer not to touch this one (e.g. not to break ABI).
   */
  void *legacy_ptr_4f1af859_c918_484a_b3f6_9fe51235a3a0;
};

bitcoinrpc_global_data_t *bitcoinrpc_global_data_;

void * (*bitcoinrpc_global_allocfunc)(size_t size) =
  bitcoinrpc_global_allocfunc_default_;

void (*bitcoinrpc_global_freefunc)(void *ptr) =
  bitcoinrpc_global_freefunc_default_;


BITCOINRPCEcode
bitcoinrpc_global_init(void)
{
  bitcoinrpc_global_data_ = NULL;
  bitcoinrpc_global_data_ = bitcoinrpc_global_allocfunc_default_(
    sizeof *bitcoinrpc_global_data_);

  if (NULL == bitcoinrpc_global_data_)
    return BITCOINRPCE_ALLOC;

  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
    {
      bitcoinrpc_global_freefunc_default_(bitcoinrpc_global_data_);
      return BITCOINRPCE_CURLE;
    }


  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_global_cleanup(void)
{
  /*
     bitcoinrpc_global_data_ was allocated with
     bitcoinrpc_global_allocfunc_default_(), so it has to be freed
     by the default as well.
   */
  if (NULL != bitcoinrpc_global_data_)
    {
      bitcoinrpc_global_freefunc_default_(bitcoinrpc_global_data_);
      bitcoinrpc_global_data_ = NULL;
    }
  curl_global_cleanup();

  return BITCOINRPCE_OK;
}


void *
bitcoinrpc_global_allocfunc_default_(size_t size)
{
  return malloc(size);
}

void
bitcoinrpc_global_freefunc_default_(void *ptr)
{
  free(ptr);
}

BITCOINRPCEcode
bitcoinrpc_global_set_allocfunc(void * (*const f)(size_t size))
{
  if (NULL == f)
    return BITCOINRPCE_ARG;
  bitcoinrpc_global_allocfunc = f;

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_global_set_freefunc(void(*const f) (void *ptr))
{
  if (NULL == f)
    return BITCOINRPCE_ARG;
  bitcoinrpc_global_freefunc = f;

  return BITCOINRPCE_OK;
}
