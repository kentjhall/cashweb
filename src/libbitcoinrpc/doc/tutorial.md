# Tutorial

In this tutorial, I will show how to start using the **bitcoinrpc** library and
make JSON-RPC calls over HTTP to a listening Bitcoin Core node. For a detailed
description of all the functions that make the bitcoinrpc interface, please
see the [Reference](./reference.md).  You may also want to check a
few [examples](./examples.md) and a document outlining the general
[design](./design.md).


## The header file

To get access to the bitcoinrpc interface, it is enough to include the main
header file:

    #include <bitcoinrpc.h>

and provide the appropriate flag to the compiler:

    -lbitcoinrpc

That alone should make possible to use the library.


## Initialising the library's global state

Before you start calling the Bitcoin RPC server demanding answers, you should
initialise global state of the library.  Simply put:

```
    bitcoinrpc_global_init();
```

once in your main thread and then go about your day.  When you want to stop
using the library, call:

```
    bitcoinrpc_global_cleanup();
```

to allow the library to collect its internal garbage before exiting.


## RPC client

To connect to the server, you do it by initialising a client structure of type
`bitcoinrpc_cl_t` and then passing it to a function performing a call.
Let's suppose, we have an active Bitcoin node waiting for RPC calls at
the local address: `127.0.0.1` and port `8332`.  The user name has been left
empty and the password is set to `bitcoinrpc` (You can set the user name and
password for your node either via command line parameters, or put it in
`bitcoin.conf` file. Please, see the `bitcoind` help page):

```

    bitcoinrpc_cl_t *cl;
    cl = bitcoinrpc_cl_init_params ("", "bitcoinrpc", "127.0.0.1", 8332);
```

If `cl != NULL`, then everything went well and we have a new client
initialised.  The last step is to define a structure that will hold an error
code and message for us.  We allocate it on stack and later pass a pointer to
it when needed:

```
    bitcoinrpc_err_t e;
```

## The API

The API of the bitcoinrpc library consist of the following data structures:

  * `bitcoinrpc_cl`
  * `bircoinrpc_method`
  * `bitcoinrpc_resp`
  * `bitcoinrpc_err`

together with the function `bitcoinrpc_call()` that takes as arguments pointers
to each of them, performs the call and saves a response from the server.

Let's define:

```

      bitcoinrpc_resp_t   *r  = NULL;
      bitcoinrpc_method_t *m  = NULL;
      json_t *params = NULL;
      json_t *j      = NULL;
```

Assume we want to set a new transaction fee to be 0.0001.  We initialise
the method `m` by:

```

      m = bitcoinrpc_method_init (BITCOINRPC_METHOD_SETTXFEE);
      if (NULL == m)
      {
        fprintf (stderr, "error: cannot initialise a new method.\n");
        exit(EXIT_FAILURE);
      }
```

and we set its parameters, i.e. the new fee, to 0.0001:

```

      params = json_array();
      json_array_append_new (params, json_real(0.0001));
      if ( bitcoinrpc_method_set_params (m, params) != BITCOINRPCE_OK)
      {
        fprintf (stderr, "error: cannot set params\n");
      }
      json_decref (params);  /* we no longer need it, the value is copied */
```

We also have to initialise a new response holder:

```

      r = bitcoinrpc_resp_init();
      if (NULL == r)
      {
        fprintf (stderr, "error: cannot initialise a new resp object.\n");
        exit(EXIT_FAILURE);
      }
```

Now it's time to make a call and check for errors:

```

      bitcoinrpc_call (cl, m, r, &e);
      if (e.code != BITCOINRPCE_OK)
      {
        fprintf (stderr, "%s\n", e.msg);
        abort();
      }
```

We can also extract the response from the server and print it:

```

      j = bitcoinrpc_resp_get (r);
      fprintf (stderr, "%s\n", json_dumps (j, JSON_INDENT(2)));
```

which should look like this:

```

      {
        "error": null,
        "result": true,
        "id": "ef661224-e620-4919-9fec-f1606d4f6545"
      }
```

for some unique `id`.

Please see the [documentation](https://jansson.readthedocs.org/en/2.7/apiref.html)
of the **jansson** library, to check how to handle data stored in
a `json_t` type.

It is important to say that the 'low-level' `bitcoinrpc_call()` just passes
the JSON data it got from the server and reports errors pertaining only to the
call itself.  Hence you have freedom to call any valid method with any
parameters, but on the other hand, it is your task to extract data from a JSON
object you receive.

*last updated: 2016-02-09*
