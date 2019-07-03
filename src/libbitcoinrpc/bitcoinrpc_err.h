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
   Routines to handle errors
 */

#ifndef BITCOINRPC_ERR_H_f341d1bf_4b5d_44d3_a772_1bde3dcbddea
#define BITCOINRPC_ERR_H_f341d1bf_4b5d_44d3_a772_1bde3dcbddea

#include "bitcoinrpc.h"

BITCOINRPCEcode
bitcoinrpc_err_set_(bitcoinrpc_err_t *e, BITCOINRPCEcode code, char* msg);

#define bitcoinrpc_RETURN return bitcoinrpc_err_set_
#define bitcoinrpc_RETURN_OK return bitcoinrpc_err_set_(e, BITCOINRPCE_OK, NULL)
#define bitcoinrpc_RETURN_ALLOC return bitcoinrpc_err_set_(e, BITCOINRPCE_ALLOC, "cannot allocate memory")

#endif /* BITCOINRPC_ERR_H_f341d1bf_4b5d_44d3_a772_1bde3dcbddea */
