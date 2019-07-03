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

#include <stdio.h>
#include <stdlib.h>

#include <jansson.h>

#include "../src/bitcoinrpc.h"
#include "../src/bitcoinrpc_resp.h"
#include "bitcoinrpc_test.h"


/* Check if can actualy create a new client */
BITCOINRPC_TESTU(resp_init)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_resp_t *r = NULL;

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");
  bitcoinrpc_resp_free(r);
  r = NULL;

  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(resp_get)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_resp_t *r = NULL;

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  json_t *j = NULL;
  json_t *jtmp = NULL;
  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j == NULL,
                    "a response object wrongly initialised");

  j = json_object();
  bitcoinrpc_resp_set_json_(r, j);
  jtmp = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(json_equal(j, jtmp),
                    "a json object: {} of resp wrongly set");
  json_decref(j);
  json_decref(jtmp);


  j = json_array();
  bitcoinrpc_resp_set_json_(r, j);
  jtmp = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(json_equal(j, jtmp),
                    "a json object: [] of resp wrongly set");
  json_decref(j);
  json_decref(jtmp);


  j = json_string("oneandonly");
  bitcoinrpc_resp_set_json_(r, j);
  jtmp = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(json_equal(j, jtmp),
                    "a json object: \"oneandonly\" of resp wrongly set");
  json_decref(j);
  json_decref(jtmp);


  j = json_null();
  bitcoinrpc_resp_set_json_(r, j);
  jtmp = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(json_equal(j, jtmp),
                    "a json object: json_null() of resp wrongly set");
  json_decref(j);
  json_decref(jtmp);


  j = NULL;
  bitcoinrpc_resp_set_json_(r, j);
  jtmp = bitcoinrpc_resp_get(r);
  /* jtmp should be just plain NULL */
  BITCOINRPC_ASSERT(j == jtmp,
                    "a json object: NULL of resp wrongly set");


  bitcoinrpc_resp_free(r);
  r = NULL;

  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(resp)
{
  BITCOINRPC_TESTU_INIT;
  BITCOINRPC_RUN_TEST(resp_init, o, NULL);
  BITCOINRPC_RUN_TEST(resp_get, o, NULL);
  BITCOINRPC_TESTU_RETURN(0);
}
