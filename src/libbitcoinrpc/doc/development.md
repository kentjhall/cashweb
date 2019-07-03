# Development
This document outlines the conventions used in the process of development of
the _bitcoinrpc_ library.  Please, try to adhere to them if you like to submit
patches to the project. On the other hand, though, do not let them stiff your
creativity, if you think there is a good reason not to take the rules
too seriously.

## Development flow
The usual flow the code is further developed and improved is through the GitHub
pull request mechanism.  I look forward for any contribution and encourage you
to send patches in the following way:

1. Fork the repository on GitHub.
2. Create a topic branch (e.g.: 2016/03/feature-name) and commit changes to
   this branch, including relevant testing.
3. Send a pull request.
4. Discuss the changes, modify further, then eventually rebase the branch
   to prepare it for merging into libbitcoinrpc/master.

Please, be descriptive in your commit messages, to the point when practically
no other information, apart from the commit messages themselves, is needed
to understand the changes.

## Testing
Every patch or pull request should introduce a new relevant test case or a good
explanation, why such a test is not needed.  Test units share the same simple
framework, based on [MinUnit](http://www.jera.com/techinfo/jtns/jtn002.html),
a minimal unit testing framework for C. A template for test units
looks as follows:

```
BITCOINRPC_TESTU(test_name)
{
  BITCOINRPC_TESTU_INIT;

  /*
  Actual test here.  Use BITCOINRPC_ASSERT(test, error_message)
  for assertions.
   */

  BITCOINRPC_TESTU_RETURN(0);
}
```

To run the test suite and then clean everything, type:

```
make test
```

If you only want to start a bitcoin daemon in regtest node and prepare
the environment for testing, please run:

```
make prep-test
```

Then, you can run the test:

```
make perform-test
```

as many times as you wish, or use additional tools like _gdb_ or _valgrind_
via `DEBUGGER` option:

```
make perform-test DEBUGGER="gdb --args"
make perform-test DEBUGGER="valgrind"
```

This will actually execute the following command:

```
DEBUGGER ./test/bitcoinrpc_test --rpc-port=18332 --rpc-password=libbitcoinrpc-test
```

After you are done with testing, type:

```
make clean-test
```

to clean everything.

During the compilation of the test application, you can set `BITCOIN_VERSION_HEX`
number to inform the program against which version of Bitcoin Core the code
is going to be tested, thus allowing it to disable e.g. deprecated RPC calls.
The definition of `BITCOIN_VERSION_HEX` is analogous as `BITCOINRPC_VERSION_HEX`;
see [Reference](./reference.md).

Refer to the project's [Makefile](../Makefile) for additional information
on how the test is performed.
