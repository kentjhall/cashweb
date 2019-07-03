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
#include "bitcoinrpc_test.h"


/* Check if can actualy create a new client */
BITCOINRPC_TESTU(client_init)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = NULL;

  cl = bitcoinrpc_cl_init();
  BITCOINRPC_ASSERT(cl != NULL,
                    "cannot initialise a new client");
  bitcoinrpc_cl_free(cl);

  cl = bitcoinrpc_cl_init_params(o.user, o.pass, o.addr, o.port);
  BITCOINRPC_ASSERT(cl != NULL,
                    "cannot initialise a new client");
  bitcoinrpc_cl_free(cl);

  /* Check if the function checks for wrong parameters */
  cl = bitcoinrpc_cl_init_params(o.user, o.pass, NULL, o.port);
  BITCOINRPC_ASSERT(cl == NULL,
                    "bitcoinrpc_cl_init_params does not check for NULLs in addr");
  bitcoinrpc_cl_free(cl);

  cl = bitcoinrpc_cl_init_params(o.user, NULL, o.addr, o.port);
  BITCOINRPC_ASSERT(cl == NULL,
                    "bitcoinrpc_cl_init_params does not check for NULLs in pass");
  bitcoinrpc_cl_free(cl);

  cl = bitcoinrpc_cl_init_params(NULL, o.pass, o.addr, o.port);
  BITCOINRPC_ASSERT(cl == NULL,
                    "bitcoinrpc_cl_init_params does not check for NULLs in user");
  bitcoinrpc_cl_free(cl);

  cl = bitcoinrpc_cl_init_params(o.user, o.pass, o.addr, 77777);
  BITCOINRPC_ASSERT(cl == NULL,
                    "bitcoinrpc_cl_init_params does not check for wrong port number");
  bitcoinrpc_cl_free(cl);

  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(client_getparams_cmdline)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = NULL;
  BITCOINRPCEcode ecode;
  char buf[BITCOINRPC_PARAM_MAXLEN];
  char buf_url[BITCOINRPC_URL_MAXLEN];
  char buf_url_cmp[BITCOINRPC_URL_MAXLEN];
  unsigned int bufi = 0;

  char user[BITCOINRPC_PARAM_MAXLEN];
  char pass[BITCOINRPC_PARAM_MAXLEN];
  char addr[BITCOINRPC_PARAM_MAXLEN];
  unsigned int port;

  /* set some parameter values */
  strncpy(user, o.user, BITCOINRPC_PARAM_MAXLEN);
  strncpy(pass, o.pass, BITCOINRPC_PARAM_MAXLEN);
  strncpy(addr, o.addr, BITCOINRPC_PARAM_MAXLEN);
  port = o.port;

  cl = bitcoinrpc_cl_init_params(user, pass, addr, port);
  BITCOINRPC_ASSERT(cl != NULL,
                    "cannot initialise a new client");


  ecode = bitcoinrpc_cl_get_user(cl, buf);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get user name");
  BITCOINRPC_ASSERT(strncmp(buf, user, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong user name received");
  memset(buf, 0, BITCOINRPC_PARAM_MAXLEN);

  ecode = bitcoinrpc_cl_get_pass(cl, buf);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get password");
  BITCOINRPC_ASSERT(strncmp(buf, pass, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong password received");
  memset(buf, 0, BITCOINRPC_PARAM_MAXLEN);

  ecode = bitcoinrpc_cl_get_addr(cl, buf);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get address");
  BITCOINRPC_ASSERT(strncmp(buf, addr, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong address received");
  memset(buf, 0, BITCOINRPC_PARAM_MAXLEN);

  ecode = bitcoinrpc_cl_get_port(cl, &bufi);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get port number");
  BITCOINRPC_ASSERT(bufi == port,
                    "wrong port number received");



  memset(buf_url, 0, BITCOINRPC_URL_MAXLEN);
  memset(buf_url_cmp, 0, BITCOINRPC_URL_MAXLEN);
  ecode = bitcoinrpc_cl_get_url(cl, buf_url);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get url");

  snprintf(buf_url_cmp, BITCOINRPC_URL_MAXLEN, "http://%s:%d", \
           addr, port);
  BITCOINRPC_ASSERT(strncmp(buf_url, buf_url_cmp, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong url received");
  memset(buf_url, 0, BITCOINRPC_URL_MAXLEN);
  memset(buf_url_cmp, 0, BITCOINRPC_URL_MAXLEN);


  bitcoinrpc_cl_free(cl);
  cl = NULL;
  BITCOINRPC_TESTU_RETURN(0);
}



BITCOINRPC_TESTU(client_getparams_edge)
{
  BITCOINRPC_TESTU_INIT;

  bitcoinrpc_cl_t *cl = NULL;
  BITCOINRPCEcode ecode;
  char buf[BITCOINRPC_PARAM_MAXLEN];
  char buf_url[BITCOINRPC_URL_MAXLEN];
  char buf_url_cmp[BITCOINRPC_URL_MAXLEN];
  unsigned int bufi = 0;

  char user[BITCOINRPC_PARAM_MAXLEN];
  char pass[BITCOINRPC_PARAM_MAXLEN];
  char addr[BITCOINRPC_PARAM_MAXLEN];
  unsigned int port;

  /* set some parameter values */
  user[0] = 0;
  strncpy(pass, "\"@!#@$", BITCOINRPC_PARAM_MAXLEN);
  strncpy(addr, "", BITCOINRPC_PARAM_MAXLEN);
  port = 1;

  cl = bitcoinrpc_cl_init_params(user, pass, addr, port);
  BITCOINRPC_ASSERT(cl != NULL,
                    "cannot initialise a new client");

  ecode = bitcoinrpc_cl_get_user(cl, buf);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get user name");
  BITCOINRPC_ASSERT(strncmp(buf, user, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong user name received");
  memset(buf, 0, BITCOINRPC_PARAM_MAXLEN);

  ecode = bitcoinrpc_cl_get_pass(cl, buf);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get password");
  BITCOINRPC_ASSERT(strncmp(buf, pass, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong password received");
  memset(buf, 0, BITCOINRPC_PARAM_MAXLEN);

  ecode = bitcoinrpc_cl_get_addr(cl, buf);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get address");
  BITCOINRPC_ASSERT(strncmp(buf, addr, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong address received");
  memset(buf, 0, BITCOINRPC_PARAM_MAXLEN);

  ecode = bitcoinrpc_cl_get_port(cl, &bufi);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get port number");
  BITCOINRPC_ASSERT(bufi == port,
                    "wrong port number received");



  memset(buf_url, 0, BITCOINRPC_URL_MAXLEN);
  memset(buf_url_cmp, 0, BITCOINRPC_URL_MAXLEN);
  ecode = bitcoinrpc_cl_get_url(cl, buf_url);
  BITCOINRPC_ASSERT(ecode == BITCOINRPCE_OK,
                    "cannot get url");

  snprintf(buf_url_cmp, BITCOINRPC_URL_MAXLEN, "http://%s:%d", \
           addr, port);
  BITCOINRPC_ASSERT(strncmp(buf_url, buf_url_cmp, BITCOINRPC_PARAM_MAXLEN) == 0,
                    "wrong url received");
  memset(buf_url, 0, BITCOINRPC_URL_MAXLEN);
  memset(buf_url_cmp, 0, BITCOINRPC_URL_MAXLEN);


  bitcoinrpc_cl_free(cl);
  cl = NULL;
  BITCOINRPC_TESTU_RETURN(0);
}


BITCOINRPC_TESTU(client)
{
  BITCOINRPC_TESTU_INIT;
  BITCOINRPC_RUN_TEST(client_init, o, NULL);
  BITCOINRPC_RUN_TEST(client_getparams_cmdline, o, NULL);
  BITCOINRPC_RUN_TEST(client_getparams_edge, o, NULL);
  BITCOINRPC_TESTU_RETURN(0);
}
