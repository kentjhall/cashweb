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

#ifndef BITCOINRPC_TEST_H_fbc8b015_1d8d_4c5c_8ec1_b4ca0a8ce138
#define BITCOINRPC_TEST_H_fbc8b015_1d8d_4c5c_8ec1_b4ca0a8ce138

#include <stdio.h>
#include "../src/bitcoinrpc.h"

#define PROGNAME "bitcoinrpc_test"

struct cmdline_options {
  char user[BITCOINRPC_PARAM_MAXLEN];
  char pass[BITCOINRPC_PARAM_MAXLEN];
  char addr[BITCOINRPC_PARAM_MAXLEN];
  unsigned int port;
};
typedef struct cmdline_options cmdline_options_t;


/*
   Test units (based on MinUnit -- a minimal unit testing framework for C,
   see: http://www.jera.com/techinfo/jtns/jtn002.html)

   Test units HAVE TO call BITCOINRPC_TESTU_INIT at the beginning
   and return with BITCOINRPC_TESTU_RETURN(0) at the end.
   Besides they assert with: BITCOINRPC_ASSERT.
   They take two parameters: cmdline_options_t and a pointer to some data
   passed to it by a unit test above.
   Test units can be nested to form test suites.
   Each test unit should be a separate program, initialising and destroying
   everything for itself, EXCEPT the library's global state.
 */

#define BITCOINRPC_TESTU(test) char* test_ ## test(cmdline_options_t o, void *testdata)

/* Dummy, so far... */
#define BITCOINRPC_TESTU_INIT \
  fprintf(stderr, "{\"test\": \"_internal_\",  \"result\": true, \"id\": %d}", tests_run++)


#define BITCOINRPC_ASSERT(test, message) \
  do { \
      if (!(test)) \
        return message; \
    } while (0)

#define BITCOINRPC_RUN_TEST(test, options, testdata)  \
  do \
    { \
      fprintf(stderr, ",\n{ \"test\": \"" # test "\", \"id\": %d, \"subtests\": [", tests_run++); \
      char *message = test_ ## test(options, testdata); \
      if (message) \
        { \
          fprintf(stderr, "], \"result\": false, \"error\": \"%s\"}", message); \
          return message; \
        } \
      else \
        { \
          fprintf(stderr, "], \"result\": true, \"error\": null}"); \
        } \
    } while (0)


#define BITCOINRPC_TESTU_RETURN(val) \
  do \
    { \
      (void)o; \
      (void)testdata; \
      return val; \
    } while (0)


extern int tests_run;


/* test names */
BITCOINRPC_TESTU(global);
BITCOINRPC_TESTU(client);
BITCOINRPC_TESTU(method);
BITCOINRPC_TESTU(resp);
BITCOINRPC_TESTU(call);
BITCOINRPC_TESTU(calln);


#endif /* BITCOINRPC_TEST_H_fbc8b015_1d8d_4c5c_8ec1_b4ca0a8ce138 */
