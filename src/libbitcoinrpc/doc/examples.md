# Examples

This is a short list of examples of how to use **bitcoinrpc** library.
For a detailed description of all the functions that make bitcoinrpc interface,
please refer to the [reference](./reference.md).  You may also want to check
the [tutorial](./tutorial.md) and a document outlining the general
[design](./design.md).


## A simple example:

```

   /* Set tx fee to 0.001 bitcoin */

    #include <jansson.h>
    #include <bitcoinrpc.h>

    int main(void)
    {

      bitcoinrpc_cl_t *cl;
      bitcoinrpc_method_t *m  = NULL;
      bitcoinrpc_resp_t   *r  = NULL;
      bitcoinrpc_err_t e;

      json_t *params = NULL;
      json_t *j      = NULL;

      bitcoinrpc_global_init();

      cl = bitcoinrpc_cl_init_params ("user", "password", "127.0.0.1", 18332);
      m = bitcoinrpc_method_init (BITCOINRPC_METHOD_SETTXFEE);
      if (NULL == m)
      {
        fprintf (stderr, "error: cannot initialise a new method.\n");
        exit(EXIT_FAILURE);
      }

      params = json_array();
      json_array_append_new (params, json_real(0.0001));
      if ( bitcoinrpc_method_set_params (m, params) != BITCOINRPCE_OK)
      {
        fprintf (stderr, "error: cannot set params\n");
      }
      json_decref (params);  /* we no longer need it, the value is copied */

      r = bitcoinrpc_resp_init();
      if (NULL == r)
      {
        fprintf (stderr, "error: cannot initialise a new resp object.\n");
        exit(EXIT_FAILURE);
      }

      bitcoinrpc_call (cl, m, r, &e);
      if (e.code != BITCOINRPCE_OK)
      {
        fprintf (stderr, "error: %s\n", e.msg);
        return 1;
      }

      j = bitcoinrpc_resp_get (r);
      fprintf (stderr, "%s\n", json_dumps (j, JSON_INDENT(2)));
      /* which should look like this:
            {
              "error": null,
              "result": true,
              "id": "ef661224-e620-4919-9fec-f1606d4f6545"
            }
      */


      bitcoinrpc_cl_free (cl);
      bitcoinrpc_global_cleanup();

      return 0;
    }
```

*last updated: 2016-02-09*
