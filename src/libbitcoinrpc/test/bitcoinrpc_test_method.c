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
#include <string.h>

#include <jansson.h>

#include "../src/bitcoinrpc.h"
#include "../src/bitcoinrpc_method.h"
#include "bitcoinrpc_test.h"


/* Check if can actualy create a new client */
BITCOINRPC_TESTU(method_init)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_method_t *m = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_HELP);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");
  bitcoinrpc_method_free(m);

  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_HELP, NULL);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method with params: NULL");
  bitcoinrpc_method_free(m);

  json_t *j = json_object();
  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_HELP, j);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method with params: {}");
  bitcoinrpc_method_free(m);
  json_decref(j);

  char invalid[1000] = "THI:S IS IN,VALI}D JS{ON DAT\"A: @@@,,@@@,,,}{{,,,";
  j = (json_t*)invalid;
  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_HELP, j);
  BITCOINRPC_ASSERT(m == NULL,
                    "bitcoinrpc_method_init_params does not check for invalid json as params");
  bitcoinrpc_method_free(m);

  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(method_params)
{
  BITCOINRPC_TESTU_INIT;

  BITCOINRPCEcode ecode;
  bitcoinrpc_method_t *m = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_HELP);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");


  json_t *jo = json_object();
  json_t *ja = json_array();
  json_t *jtmp = NULL;

  ecode = bitcoinrpc_method_set_params(m, NULL);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot set NULL as params");

  jtmp = NULL;
  ecode = bitcoinrpc_method_get_params(m, &jtmp);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get params (NULL)");

  BITCOINRPC_ASSERT(jtmp == NULL,
                    "bitcoinrpc_method_get_params did not returned set parameters");



  ecode = bitcoinrpc_method_set_params(m, jo);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot set empty object as params");

  jtmp = NULL;
  ecode = bitcoinrpc_method_get_params(m, &jtmp);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get params ({})");

  BITCOINRPC_ASSERT(json_equal(jo, jtmp),
                    "bitcoinrpc_method_get_params did not returned set parameters");


  json_decref(jtmp);
  jtmp = NULL;

  ecode = bitcoinrpc_method_set_params(m, ja);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot set empty array as params");

  ecode = bitcoinrpc_method_get_params(m, &jtmp);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get params ([])");

  BITCOINRPC_ASSERT(json_equal(ja, jtmp),
                    "bitcoinrpc_method_get_params did not returned set parameters");


  bitcoinrpc_method_free(m);
  json_decref(ja);
  json_decref(jo);
  json_decref(jtmp);

  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(method_set_nonstandard)
{
  BITCOINRPC_TESTU_INIT;

  BITCOINRPCEcode ecode;
  bitcoinrpc_method_t *m = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_SETTXFEE);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");


  ecode = bitcoinrpc_method_set_nonstandard(m, "hereandnow");
  BITCOINRPC_ASSERT(ecode != BITCOINRPCE_OK,
                    "a nonstandard method name set to method other than BITCOINRPC_METHOD_NONSTANDARD");

  BITCOINRPC_ASSERT(strncmp(m->mstr, "settxfee", 8) == 0,
                    "a nonstandard method name has been set anyway");

  bitcoinrpc_method_free(m);


  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_NONSTANDARD);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");


  ecode = bitcoinrpc_method_set_nonstandard(m, "oneandonly");
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot set a nonstandard method name");

  BITCOINRPC_ASSERT(strncmp(m->mstr, "oneandonly", 10) == 0,
                    "a nonstandard method name wrongly set");

  bitcoinrpc_method_free(m);
  m = NULL;

  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(method)
{
  BITCOINRPC_TESTU_INIT;
  BITCOINRPC_RUN_TEST(method_init, o, NULL);
  BITCOINRPC_RUN_TEST(method_params, o, NULL);
  BITCOINRPC_RUN_TEST(method_set_nonstandard, o, NULL);
  BITCOINRPC_TESTU_RETURN(0);
}
