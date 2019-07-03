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
#include <string.h>
#include <curl/curl.h>
#include <uuid/uuid.h>

#include "bitcoinrpc.h"
#include "bitcoinrpc_cl.h"
#include "bitcoinrpc_global.h"


/*
   Internal stuff
 */

# define bitcoinrpc_cl_update_url_(cl) \
  snprintf(cl->url, BITCOINRPC_URL_MAXLEN, "http://%s:%d", \
           cl->addr, cl->port);


/* ------------------------------------------------------------------------ */

bitcoinrpc_cl_t*
bitcoinrpc_cl_init(void)
{
  return bitcoinrpc_cl_init_params(BITCOINRPC_USER_DEFAULT,
                                   BITCOINRPC_PASS_DEFAULT,
                                   BITCOINRPC_ADDR_DEFAULT,
                                   BITCOINRPC_PORT_DEFAULT);
}


bitcoinrpc_cl_t*
bitcoinrpc_cl_init_params(const char* user, const char* pass,
                          const char* addr, const unsigned int port)
{
  if (NULL == user || NULL == pass || NULL == addr || port <= 0 || port > 65535)
    return NULL;

  bitcoinrpc_cl_t *cl = bitcoinrpc_global_allocfunc(sizeof *cl);

  if (NULL == cl)
    return NULL;

  /* Initialise all the elemets of cl */
  memset(cl->uuid_str, 0, 37);
  memset(cl->user, 0, BITCOINRPC_PARAM_MAXLEN);
  memset(cl->pass, 0, BITCOINRPC_PARAM_MAXLEN);
  memset(cl->addr, 0, BITCOINRPC_PARAM_MAXLEN);
  cl->port = 0;
  memset(cl->url, 0, BITCOINRPC_URL_MAXLEN);
  cl->curl = NULL;
  cl->curl_headers = NULL;
  cl->legacy_ptr_4f1af859_c918_484a_b3f6_9fe51235a3a0 = NULL;

  uuid_generate_random(cl->uuid);
  uuid_unparse_lower(cl->uuid, cl->uuid_str);

  /* make room for terminating '\0' */
  strncpy(cl->user, user, BITCOINRPC_PARAM_MAXLEN - 1);

  strncpy(cl->pass, pass, BITCOINRPC_PARAM_MAXLEN - 1);
  strncpy(cl->addr, addr, BITCOINRPC_PARAM_MAXLEN - 1);
  cl->port = port;

  bitcoinrpc_cl_update_url_(cl);

  cl->curl = curl_easy_init();
  if (NULL == cl->curl)
    {
      bitcoinrpc_global_freefunc(cl);
      return NULL;
    }

  cl->curl_headers = curl_slist_append(cl->curl_headers, "content-type: text/plain;");
  if (NULL == cl->curl_headers)
    {
      bitcoinrpc_global_freefunc(cl);
      return NULL;
    }

  curl_easy_setopt(cl->curl, CURLOPT_HTTPHEADER, cl->curl_headers);

  return cl;
}


BITCOINRPCEcode
bitcoinrpc_cl_free(bitcoinrpc_cl_t *cl)
{
  if (NULL == cl)
    return BITCOINRPCE_ARG;

  curl_slist_free_all(cl->curl_headers);
  curl_easy_cleanup(cl->curl);
  cl->curl = NULL;
  bitcoinrpc_global_freefunc(cl);
  cl = NULL;

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_cl_get_user(bitcoinrpc_cl_t *cl, char *buf)
{
  if (NULL == cl || NULL == buf)
    return BITCOINRPCE_ARG;
  strncpy(buf, cl->user, BITCOINRPC_PARAM_MAXLEN);

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_cl_get_pass(bitcoinrpc_cl_t *cl, char *buf)
{
  if (NULL == cl || NULL == buf)
    return BITCOINRPCE_ARG;
  strncpy(buf, cl->pass, BITCOINRPC_PARAM_MAXLEN);

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_cl_get_addr(bitcoinrpc_cl_t *cl, char *buf)
{
  if (NULL == cl || NULL == buf)
    return BITCOINRPCE_ARG;
  strncpy(buf, cl->addr, BITCOINRPC_PARAM_MAXLEN);

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_cl_get_port(bitcoinrpc_cl_t *cl, unsigned int *bufi)
{
  if (NULL == cl || NULL == bufi)
    return BITCOINRPCE_ARG;
  *bufi = cl->port;

  return BITCOINRPCE_OK;
}


BITCOINRPCEcode
bitcoinrpc_cl_get_url(bitcoinrpc_cl_t *cl, char *buf)
{
  bitcoinrpc_cl_update_url_(cl);  /* one never knows */

  if (NULL == cl || NULL == buf)
    return BITCOINRPCE_ARG;
  strncpy(buf, cl->url, BITCOINRPC_URL_MAXLEN);

  return BITCOINRPCE_OK;
}
