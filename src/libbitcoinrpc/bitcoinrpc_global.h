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
   Global data internal to library (not accesed by a user).
 */

#ifndef BITCOINRPC_GLOBAL_H_5fe378f8_8280_4f1c_a3c3_7c84da05eff5
#define BITCOINRPC_GLOBAL_H_5fe378f8_8280_4f1c_a3c3_7c84da05eff5

#include <stdlib.h>
#include "bitcoinrpc.h"

struct bitcoinrpc_global_data_s_;

typedef
struct bitcoinrpc_global_data_s_
bitcoinrpc_global_data_t;

extern bitcoinrpc_global_data_t *bitcoinrpc_global_data_;


/* The default memory allocating function used by the library */
void *
bitcoinrpc_global_allocfunc_default_(size_t size);

/* The default memory freeing function used by the library */
void
bitcoinrpc_global_freefunc_default_(void *ptr);

/*
   The memory allocating function used by the library
   (the default is just standard malloc() ).
 */
extern void * (*bitcoinrpc_global_allocfunc)(size_t size);

/*
   The memory freeing function used by the library
   (the default is just standard free() ).
 */
extern void (* bitcoinrpc_global_freefunc)(void *ptr);


#endif /* BITCOINRPC_GLOBAL_H_5fe378f8_8280_4f1c_a3c3_7c84da05eff5 */
