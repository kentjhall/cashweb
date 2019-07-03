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
#include <math.h>

#include <jansson.h>

#include "../src/bitcoinrpc.h"
#include "../src/bitcoinrpc_method.h"
#include "bitcoinrpc_test.h"


BITCOINRPC_TESTU(call_getconnectioncount)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_GETCONNECTIONCOUNT);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_integer(jresult),
                    "getconnectioncount value is not an integer");

  json_decref(j);

  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(call_getblock)
{
  BITCOINRPC_TESTU_INIT;
  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_method_t *m2 = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *j2 = NULL;
  json_t *jresult = NULL;
  json_t *jresult2 = NULL;
  json_t *jparams = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_GETBESTBLOCKHASH);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);
  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_string(jresult),
                    "getbestblockhash value is not a string");


  jparams = json_array();
  json_array_append(jparams, jresult);
  json_array_append_new(jparams, json_false());

  m2 = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_GETBLOCK, jparams);
  BITCOINRPC_ASSERT(m2 != NULL,
                    "cannot initialise a new method");

  bitcoinrpc_call(cl, m2, r, &e);
  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call: getbestblockhash");

  j2 = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j2 != NULL,
                    "cannot parse response from the server: getblock");

  jresult2 = json_object_get(j2, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_string(jresult2),
                    "getblock value is not a string");

  //fprintf(stderr, "\n\n\n%s\n\n\n", json_string_value(jresult2));

  json_decref(j2);
  json_decref(j);
  json_decref(jparams);


  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m2);
  bitcoinrpc_method_free(m);

  BITCOINRPC_TESTU_RETURN(0);
}


/* Warning: this method is deprecated! */
BITCOINRPC_TESTU(call_getinfo)
{
  BITCOINRPC_TESTU_INIT;
  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_GETINFO);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_object(jresult),
                    "getinfo value is not an object");

  /* Is it really getinfo ?*/
  BITCOINRPC_ASSERT(json_object_del(jresult, "version") != -1,
                    "getinfo has not \"version\" key; is it really getinfo?");

  BITCOINRPC_ASSERT(json_object_del(jresult, "testnet") != -1,
                    "getinfo has not \"testnet\" key; is it really getinfo?");

  BITCOINRPC_ASSERT(json_object_del(jresult, "connections") != -1,
                    "getinfo has not \"connections\" key; is it really getinfo?");

  json_decref(j);

  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(call_settxfee)
{
  BITCOINRPC_TESTU_INIT;
  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *jerr = NULL;
  json_t *jparams = NULL;
  jparams = json_array();

#if BITCOIN_VERSION_HEX < 0x001200
  const double fee = 0.02341223;
  json_array_append_new(jparams, json_real(fee));
#else
  const char* fee = "0.02341223";
  json_array_append_new(jparams, json_string(fee));
#endif

  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_SETTXFEE, jparams);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");

  json_decref(j);
  json_decref(jparams);
  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);


  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_GETINFO);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_object(jresult),
                    "getinfo value is not an object");

#if BITCOIN_VERSION_HEX < 0x001200
  BITCOINRPC_ASSERT(json_real_value(json_object_get(jresult, "paytxfee")) == fee,
                    "the tx fee wrongly set");
#else
  BITCOINRPC_ASSERT(json_real_value(json_object_get(jresult, "paytxfee")) == atof(fee),
                    "the tx fee wrongly set");
#endif

  json_decref(j);
  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(call_settxfee47)
{
  BITCOINRPC_TESTU_INIT;
  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *jerr = NULL;
  json_t *jparams = NULL;

  const size_t n = 47;

  for (size_t i = 0; i < n; i++)
    {
      jparams = json_array();
#if BITCOIN_VERSION_HEX < 0x001200
      const double fee = 0.02300601;
      json_array_append_new(jparams, json_real(fee));
#else
      const char* fee = "0.02300601";
      json_array_append_new(jparams, json_string(fee));
#endif

      m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_SETTXFEE, jparams);
      BITCOINRPC_ASSERT(m != NULL,
                        "cannot initialise a new method");

      r = bitcoinrpc_resp_init();
      BITCOINRPC_ASSERT(r != NULL,
                        "cannot initialise a new response");

      bitcoinrpc_call(cl, m, r, &e);

      BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                        "cannot perform a call");

      j = bitcoinrpc_resp_get(r);
      BITCOINRPC_ASSERT(j != NULL,
                        "cannot parse response from the server");

      jerr = json_object_get(j, "error");
      BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                        "the server returned non zero error code");

      json_decref(j);
      json_decref(jparams);
      bitcoinrpc_resp_free(r);
      bitcoinrpc_method_free(m);


      m = bitcoinrpc_method_init(BITCOINRPC_METHOD_GETINFO);
      BITCOINRPC_ASSERT(m != NULL,
                        "cannot initialise a new method");

      r = bitcoinrpc_resp_init();
      BITCOINRPC_ASSERT(r != NULL,
                        "cannot initialise a new response");

      bitcoinrpc_call(cl, m, r, &e);

      BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                        "cannot perform a call");

      j = bitcoinrpc_resp_get(r);
      BITCOINRPC_ASSERT(j != NULL,
                        "cannot parse response from the server");

      jerr = json_object_get(j, "error");
      BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                        "the server returned non zero error code");
      json_decref(jerr);

      json_t *jresult = json_object_get(j, "result");
      BITCOINRPC_ASSERT(jresult != NULL,
                        "the response has no key: \"result\"");

      BITCOINRPC_ASSERT(json_is_object(jresult),
                        "getinfo value is not an object");

#if BITCOIN_VERSION_HEX < 0x001200
      BITCOINRPC_ASSERT(json_real_value(json_object_get(jresult, "paytxfee")) == fee,
                        "the tx fee wrongly set");
#else
      BITCOINRPC_ASSERT(json_real_value(json_object_get(jresult, "paytxfee")) == atof(fee),
                        "the tx fee wrongly set");
#endif

      json_decref(j);
      bitcoinrpc_resp_free(r);
      bitcoinrpc_method_free(m);
    }

  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(call_getbalance_noparams)
{
  BITCOINRPC_TESTU_INIT;
  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *jparams = NULL;

  m = bitcoinrpc_method_init(BITCOINRPC_METHOD_GETBALANCE);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_real(jresult),
                    "getbalance value is not an real number");


  json_decref(jparams);
  json_decref(j);
  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(call_getbalance_minconf10)
{
  BITCOINRPC_TESTU_INIT;
  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *jparams = NULL;

  jparams = json_array();
  json_array_append_new(jparams, json_string(""));
  json_array_append_new(jparams, json_integer(10));

  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_GETBALANCE, jparams);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_real(jresult),
                    "getbalance value is not an real number");


  json_decref(jparams);
  json_decref(j);
  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(call_generate0)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *jparams = NULL;

  size_t n = 0;

  jparams = json_array();
  json_array_append_new(jparams, json_integer(n));

  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_GENERATE, jparams);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_array(jresult),
                    "generate method result is not an array");

  BITCOINRPC_ASSERT(json_array_size(jresult) == n,
                    "the array returned is of wrong size");

  json_decref(jparams);
  json_decref(j);
  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(call_generate99)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = (bitcoinrpc_cl_t*)testdata;
  bitcoinrpc_method_t *m = NULL;
  bitcoinrpc_resp_t *r = NULL;
  bitcoinrpc_err_t e;
  json_t *j = NULL;
  json_t *jparams = NULL;

  size_t n = 99;

  jparams = json_array();
  json_array_append_new(jparams, json_integer(n));

  m = bitcoinrpc_method_init_params(BITCOINRPC_METHOD_GENERATE, jparams);
  BITCOINRPC_ASSERT(m != NULL,
                    "cannot initialise a new method");

  r = bitcoinrpc_resp_init();
  BITCOINRPC_ASSERT(r != NULL,
                    "cannot initialise a new response");

  bitcoinrpc_call(cl, m, r, &e);

  BITCOINRPC_ASSERT(e.code == BITCOINRPCE_OK,
                    "cannot perform a call");

  j = bitcoinrpc_resp_get(r);
  BITCOINRPC_ASSERT(j != NULL,
                    "cannot parse response from the server");

  json_t *jerr = json_object_get(j, "error");
  BITCOINRPC_ASSERT(json_equal(jerr, json_null()),
                    "the server returned non zero error code");
  json_decref(jerr);

  json_t *jresult = json_object_get(j, "result");
  BITCOINRPC_ASSERT(jresult != NULL,
                    "the response has no key: \"result\"");

  BITCOINRPC_ASSERT(json_is_array(jresult),
                    "generate method result is not an array");

  BITCOINRPC_ASSERT(json_array_size(jresult) == n,
                    "the array returned is of wrong size");

  json_decref(jparams);
  json_decref(j);
  bitcoinrpc_resp_free(r);
  bitcoinrpc_method_free(m);
  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(call)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = NULL;

  cl = bitcoinrpc_cl_init_params(o.user, o.pass, o.addr, o.port);
  BITCOINRPC_ASSERT(cl != NULL,
                    "cannot initialise a new client");


  /* Perform test with the same client */
  BITCOINRPC_RUN_TEST(call_getconnectioncount, o, cl);
  BITCOINRPC_RUN_TEST(call_getblock, o, cl);
  BITCOINRPC_RUN_TEST(call_getinfo, o, cl);
  BITCOINRPC_RUN_TEST(call_settxfee, o, cl);
  BITCOINRPC_RUN_TEST(call_settxfee47, o, cl);
  BITCOINRPC_RUN_TEST(call_getbalance_noparams, o, cl);
  BITCOINRPC_RUN_TEST(call_getbalance_minconf10, o, cl);

#if BITCOIN_VERSION_HEX >= 0x001100
  BITCOINRPC_RUN_TEST(call_generate0, o, cl);
  BITCOINRPC_RUN_TEST(call_generate99, o, cl);
#endif /* BITCOIN_VERSION_HEX >= 0x001100 */

  bitcoinrpc_cl_free(cl);
  cl = NULL;

  BITCOINRPC_TESTU_RETURN(0);
}
