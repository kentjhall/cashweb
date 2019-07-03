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
   Internal stuff for client
 */

#ifndef BITCOINRPC_CL_H_6b1e267b_bbce_4a84_8a18_172da32608a5
#define BITCOINRPC_CL_H_6b1e267b_bbce_4a84_8a18_172da32608a5

#include <curl/curl.h>
#include <uuid/uuid.h>
#include "bitcoinrpc.h"

struct bitcoinrpc_cl {
  uuid_t uuid;
  char uuid_str[37];  /* man 3 uuid_unparse */

  char user[BITCOINRPC_PARAM_MAXLEN];
  char pass[BITCOINRPC_PARAM_MAXLEN];
  char addr[BITCOINRPC_PARAM_MAXLEN];
  unsigned int port;

  char url[BITCOINRPC_URL_MAXLEN];

  CURL *curl;
  struct curl_slist *curl_headers;

  /*
     This is a legacy pointer. You can point to an auxilliary structure,
     if you prefer not to touch this one (e.g. not to break ABI).
   */
  void *legacy_ptr_4f1af859_c918_484a_b3f6_9fe51235a3a0;
};


#endif /* BITCOINRPC_CL_H_6b1e267b_bbce_4a84_8a18_172da32608a5 */
