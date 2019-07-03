# Documentation

This is the main documentation file of **bitcoinrpc**, a C language library
for JSON-RPC Bitcoin Core API.

The library provides basic routines to send RPC queries to a listening
Bitcoin Core node, fetch responses and analyse errors.
Its main features include:

* Reusable components, allowing to perform many queries through one open
  connection, as well as to query many listening servers with the same method
  without reallocating resources.
* Proper error handling.

To quickly get a grasp of how to use the library in your code, please see
the [tutorial](./tutorial.md) and [examples](./examples.md).  Besides, the
following documents provide more information:

* [Design](./design.md) -- Overview of the design behind the library.
* [Development](./development.md) -- Coding conventions used for this library.
* [Reference](./reference.md) -- Full ABI and API reference.

Instructions how to build and install the library can be found in
`README.md` file, in the project main directory.

After you have installed the software, you should be able to browse its man
pages offline, which are basically the above reference document adjusted to
the man pages format. Just type:

    man 3 bitcoinrpc


### Final note

As you have detected, the main author of this software and its documentation
is not a native English speaker.  Hence, he will highly appreciate any
suggestions of how to improve this text in terms of spelling, grammar, syntax
etc.  Please send proposals as github pull requests.  Any commit message
regarding documentation should bear: `[doc]` prefix.

*last updated: 2016-02-08*
