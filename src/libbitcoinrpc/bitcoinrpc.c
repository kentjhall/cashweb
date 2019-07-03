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

#include <string.h>

#include <curl/curl.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include "bitcoinrpc.h"
#include "bitcoinrpc_cl.h"
#include "bitcoinrpc_err.h"
#include "bitcoinrpc_global.h"
#include "bitcoinrpc_method.h"
#include "bitcoinrpc_resp.h"



struct bitcoinrpc_call_curl_resp_ {
  char* data;
  unsigned long long int data_len;
  int called_before;
  bitcoinrpc_err_t e;
};


size_t
bitcoinrpc_call_write_callback_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  size_t n = size * nmemb;
  struct bitcoinrpc_call_curl_resp_ *curl_resp = (struct bitcoinrpc_call_curl_resp_*)userdata;

  if (!curl_resp->called_before)
    {
      /* initialise the data structure */
      curl_resp->called_before = 1;
      curl_resp->data_len = 0;
      curl_resp->data = NULL;
      curl_resp->e.code = BITCOINRPCE_OK;
    }

  char * data = NULL;
  unsigned long long int data_len = curl_resp->data_len + n + 1;
  data = bitcoinrpc_global_allocfunc(data_len);
  if (NULL == data)
    {
      if (curl_resp->data != NULL)
        bitcoinrpc_global_freefunc(curl_resp->data);
      curl_resp->e.code = BITCOINRPCE_ALLOC;
      snprintf(curl_resp->e.msg, BITCOINRPC_ERRMSG_MAXLEN,
               "cannot allocate more memory");
      return 0;
    }
  /* concatenate old and new data */
  if (NULL != curl_resp->data)
    {
      /* do not copy last '\0' */
      size_t i;
      for (i = 0; i < curl_resp->data_len; i++)
        {
          data[i] = curl_resp->data[i];
        }
    }

  /* do not copy '\n' */
  size_t i, j;
  for (i = 0, j = 0; i < n; i++)
    {
      if (ptr[i] != '\n')
        data[curl_resp->data_len + j++] = ptr[i];
    }
  data[curl_resp->data_len + j] = '\0';

  if (NULL != curl_resp->data)
    {
      bitcoinrpc_global_freefunc(curl_resp->data);
    }

  curl_resp->data = data;
  curl_resp->data_len += j;

  return n;
}


BITCOINRPCEcode
bitcoinrpc_call(bitcoinrpc_cl_t * cl, bitcoinrpc_method_t * method,
                bitcoinrpc_resp_t *resp, bitcoinrpc_err_t *e)
{
  if (NULL == cl || NULL == method || NULL == resp)
    return BITCOINRPCE_ARG;

  return bitcoinrpc_calln(cl, 1, &method, &resp, e);
}



BITCOINRPCEcode
bitcoinrpc_calln(bitcoinrpc_cl_t *cl, size_t n, bitcoinrpc_method_t **methods,
                 bitcoinrpc_resp_t **resps, bitcoinrpc_err_t *e)

{
  json_t *j = NULL;
  json_t *jtmp = NULL;
  char *data = NULL;
  char url[BITCOINRPC_URL_MAXLEN];
  char user[BITCOINRPC_PARAM_MAXLEN];
  char pass[BITCOINRPC_PARAM_MAXLEN];
  char credentials[2 * BITCOINRPC_PARAM_MAXLEN + 1];
  struct bitcoinrpc_call_curl_resp_ curl_resp;
  BITCOINRPCEcode ecode;
  CURLcode curl_err;
  char errbuf[BITCOINRPC_ERRMSG_MAXLEN];
  char curl_errbuf[CURL_ERROR_SIZE];

  if (NULL == cl || NULL == methods || NULL == resps)
    return BITCOINRPCE_ARG;

  /* make sure the error message will not be trash */
  *(e->msg) = '\0';

  j = json_array();
  if (NULL == j)
    bitcoinrpc_RETURN(e, BITCOINRPCE_JSON, "JSON error while creating a new json_array");

  for (size_t i = 0; i < n; i++)
    {
      jtmp = json_object();
      if (NULL == jtmp)
        bitcoinrpc_RETURN(e, BITCOINRPCE_JSON, "JSON error while creating a new json_object");

      json_object_set_new(jtmp, "jsonrpc", json_string("2.0"));
      json_object_update(jtmp, bitcoinrpc_method_get_postjson_(methods[i]));
      json_array_append_new(j, jtmp);
    }

  data = json_dumps(j, JSON_COMPACT);
  if (NULL == data)
    bitcoinrpc_RETURN(e, BITCOINRPCE_JSON, "JSON error while writing POST data");

  if (NULL == cl->curl)
    bitcoinrpc_RETURN(e, BITCOINRPCE_BUG, "this should not happen; please report a bug");

  curl_easy_setopt(cl->curl, CURLOPT_POSTFIELDSIZE, (long)strlen(data));
  curl_easy_setopt(cl->curl, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(cl->curl, CURLOPT_WRITEFUNCTION, bitcoinrpc_call_write_callback_);
  curl_resp.called_before = 0;
  curl_easy_setopt(cl->curl, CURLOPT_WRITEDATA, &curl_resp);

  ecode = bitcoinrpc_cl_get_url(cl, url);

  if (ecode != BITCOINRPCE_OK)
    bitcoinrpc_RETURN(e, BITCOINRPCE_BUG, "url malformed; please report a bug");
  curl_easy_setopt(cl->curl, CURLOPT_URL, url);

  bitcoinrpc_cl_get_user(cl, user);
  bitcoinrpc_cl_get_pass(cl, pass);
  snprintf(credentials, 2 * BITCOINRPC_PARAM_MAXLEN + 1,
           "%s:%s", user, pass);
  curl_easy_setopt(cl->curl, CURLOPT_USERPWD, credentials);

  curl_easy_setopt(cl->curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
  curl_easy_setopt(cl->curl, CURLOPT_ERRORBUFFER, curl_errbuf);
  curl_err = curl_easy_perform(cl->curl);

  json_decref(j); /* no longer needed */
  free(data);

  if (curl_err != CURLE_OK)
    {
      snprintf(errbuf, BITCOINRPC_ERRMSG_MAXLEN, "curl error: %s", curl_errbuf);
      bitcoinrpc_RETURN(e, BITCOINRPCE_CURLE, errbuf);
    }

  /* Check if server returned valid json object */
  if (curl_resp.e.code != BITCOINRPCE_OK)
    {
      bitcoinrpc_RETURN(e, BITCOINRPCE_CON, curl_resp.e.msg);
    }

  /* parse read data into json */
  json_error_t jerr;
  j = NULL;
  j = json_loads(curl_resp.data, 0, &jerr);
  if (NULL == j)
    {
      snprintf(errbuf, BITCOINRPC_ERRMSG_MAXLEN,
               "cannot parse JSON data from the server: %s", curl_resp.data);
      bitcoinrpc_RETURN(e, BITCOINRPCE_CURLE, errbuf);
    }

  for (size_t i = 0; i < n; i++)
    {
      jtmp = json_array_get(j, i);
      if (NULL == jtmp)
        {
          bitcoinrpc_RETURN(e, BITCOINRPCE_JSON, "cannot parse data returned from the server");
        }
      bitcoinrpc_resp_set_json_(resps[i], jtmp);
    }

  bitcoinrpc_global_freefunc(curl_resp.data);
  json_decref(j);

  for (size_t i = 0; i < n; i++)
    {
      if (bitcoinrpc_resp_check(resps[i], methods[i])
          != BITCOINRPCE_OK)
        bitcoinrpc_RETURN(e, BITCOINRPCE_CHECK,
                          "at least one response id does not match corresponding post id");
    }
  bitcoinrpc_RETURN_OK;
}
