# Design

This document presents the design behind the **bitcoinrpc** library.
The main purpose of the library is to provide a convenient and robust
interface to handle remote procedure calls (RPCs) to Bitcoin Core nodes.
This way, you can obtain information about the current status of a node and
the network, as well as to control its behaviour. The are many RPC methods
a Bitcoin node accepts, ranging from getting the number of connected nodes to
actually sending money over the network. By using bitcoinrpc, you can send
RPC calls to the node, receive the responses and handle errors in a robust way.


## Overview of the Bitcoin JSON-RPC protocol.

A Bitcoin Core node, when stated as a daemon of with `-server` option, listens
to incoming connection requests to perform RPCs.  The port the node
usually listens to is `8332` for mainnet and `18332` for testnet and regtest
mode, but you can specify the port yourself via `-rcpport` option.  You should
also give the server a user name (optionally) and a password for basic
authentication, either via command option, or in `bitcoin.conf` file.
For more information, please refer to the `bitcoind` help page.

The RPC call is performed over HTTP and the data sent and received is in JSON
format. For the specification of the JSON-RPC protocol, see:
[http://json-rpc.org/](http://json-rpc.org/).
For the specification of the PRC protocol Bitcoin Core uses, see Bitcoin Core
[Developer Reference](https://bitcoin.org/en/developer-reference#remote-procedure-calls-rpcs).

So the main task of bitcoinrpc it to initiate a connection to a listing Bitcoin
node, compose a valid JSON-RPC request and send it over HTTP with basic
authentication; then wait for the response, extract the JSON data of interest
and report it to the user, together with possible error code and message.

It is important to say that the purpose of bitcoinrpc is to serve as a proxy
between the user and the Bitcoin server, allowing him to sent *almost any*
request (i.e. to specify any parameters for a valid RPC method) and report
errors relevant only to this process of communication and not to check,
if the request has been interpreted by the Bitcoin node as meaningful.
This is indeed the default behaviour of the low-level bitcoinrpc routines.
See the library [reference](./reference.md) for more details.


## Library dependencies

The bitcoinrpc library is written in standard C.  It sends data over HTTP with
the help of `libcurl_easy` interface.  Hence, the necessary dependency:
`libcurl`.  Please see:
[Easy interface overview](http://curl.haxx.se/libcurl/c/libcurl-easy.html),
if you need more information, but keep in mind that the user of bitcoinrpc does
not have to bother with curl routines, as they are hidden completely behind
the interface.  Additionally, the library parses JSON data using `libjansson`
and some aspects of its API are exposed to the user.  So it could be
helpful, if you get familiar with libjansson excellent documentation:
[here](https://jansson.readthedocs.org/en/2.7/apiref.html).
The last library needed to compile bitcoinrpc is standard
[`libuuid`](http://linux.die.net/man/3/libuuid), again hidden deep inside.


## The design

The library is designed around four independent data structures:

* `bitcoinrpc_cl`     -- RPC client, handling connection to a node
* `bircoinrpc_method` -- a method to be sent
* `bitcoinrpc_resp`   -- a JSON response from the server
* `bitcoinrpc_err`    -- error handling

In addition, there is the function `bitcoinrpc_call()` that performs the call.
This 'low-level' function just passes the JSON data it got from the server
and reports errors pertaining only to the call itself.  
It allows to call the server many times using the same `bitcoinrpc_method`,
thus saving time and memory. It is also possible to use the same method to
call many servers, collecting responses either to separate structures or
to the same `bitcoinrpc_resp` in sequence.

The data structures are independent as they do not share memory. That probably
makes bitcoinrpc rather thread-safe, although it still waits for proper testing
in this regard.


## Error messages

Most of the functions within the library perform trivial task such as
allocating memory etc., and they do not need to return extensive error messages.
Instead, they return error codes of enum type `BITCOINRPCEcode`.
The list of all error codes can be found in the [reference](./reference.md).
The function `bitcoinrpc_call()` accepts an additional pointer to
`bitcoinrcp_err_t`, where error messages are stored. If the pointer is passed
as `NULL`, the error reporting, beside the omnipresent error codes, is omitted.


## Usage of bitcoinrpc

The standard procedure of using bitcoinrpc library routines to perform
a successful RPC call should therefore look as follows:

1. Initialise the library once by calling `bitcoinrpc_global_init()`.
2. Initialise a client: `bitcoinrpc_cl_init()` and specify parameters like
   user name, password, IP address and port of the server.
3. Initialise method and response structures. Also allocate on stack the error
   handling object.
4. Perform a call: `bitcoinrpc_call()`.
5. Check for errors.
6. Unpack the response in JSON format.
7. Perform steps 4.-6. as many times as needed (possibly with a different
   method).
8. Free the initialised structures via appropriate `bitcointpc_*_free()`.
9. Clean up the library's internal state: `bitcoinrpc_global_cleanup()`.

For more specific explanation of how to you the library, please refer to
the [tutorial](./tutorial.md) and [examples](./examples.md).

*last updated: 2016-02-20*
