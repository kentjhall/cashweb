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
#include <getopt.h>
#include <string.h>

#include <jansson.h>

#include "../src/bitcoinrpc.h"
#include "bitcoinrpc_test.h"


void
print_help_page(void)
{
  printf("%s %s\n", PROGNAME, BITCOINRPC_VERSION);
  printf("Usage: %s [OPTIONS] ARG1 [ARG2, ARG3, ...]\n", PROGNAME);
  printf("\n"
         " Options:\n"
         "    -h, --help                  Print this help page and exit\n"
         "    -V, --BITCOINRPC_VERSION               Print BITCOINRPC_VERSION number and exit\n"
         "\n"
         "  Bitcoin Core brpc credentials:\n"
         "    --rpc-address=ADDRESS       IP address of the bitcoind server listening to\n"
         "                                incoming RPC calls (default 127.0.0.1)\n"
         "    --rpc-port=PORT             Port of the bitcoind node (default 8332)\n"
         "    --rpc-user=USER             User name for RPC calls (default: user)\n"
         "    --rpc-password=PASS         Password for RPC calls (dafault: password)\n"
         "                                See your bitcoin.conf file.\n"
         "\n"
         "Written by Marek Miller and others, see CREDITS file.\n"
         "Report bugs to marek.l.miller@gmail.com\n"
         "\n"
         "License: MIT (see https://opensource.org/licenses/MIT).\n"
         "The software is provided \"as is\", without warranty of any kind.\n"
         );
}



int parse_command_options(int argc, char **argv, cmdline_options_t *o)
{
  int c;

  int addr_flag = 0;
  int user_flag = 0;
  int port_flag = 0;
  int pass_flag = 0;

  while (1)
    {
      struct option long_options[] =
      {
        /* These options set a flag. */
        /* {"verbose",       no_argument,  &verbose_flag, 1}, */
        { "rpc-address", required_argument, &addr_flag, 1 },
        { "rpc-password", required_argument, &pass_flag, 1 },
        { "rpc-port", required_argument, &port_flag, 1 },
        { "rpc-user", required_argument, &user_flag, 1 },
        /* Long-only options. */

        /* These options don’t set a flag.
           We distinguish them by their indices. */
        { "help", no_argument, 0, 'h' },
        { "BITCOINRPC_VERSION", no_argument, 0, 'V' },
        { 0, 0, 0, 0 }
      };
      /* getopt_long stores the option index here. */
      int option_index = 0;

      c = getopt_long(argc, argv, "a:p:hV",
                      long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c)
        {
        case 0:

          if (strncmp("rpc-user", long_options[option_index].name, 8) == 0)
            {
              if (strlen(optarg) >= BITCOINRPC_PARAM_MAXLEN)
                {
                  fprintf(stderr, "error: The argument to --rpc-user is longer than %d characters.\n Either specify a shorter user name, or change the source code.\n", BITCOINRPC_PARAM_MAXLEN);
                  return 1;
                }
              strncpy(o->user, optarg, BITCOINRPC_PARAM_MAXLEN);
            }

          if (strncmp("rpc-password", long_options[option_index].name, 12) == 0)
            {
              if (strlen(optarg) > BITCOINRPC_PARAM_MAXLEN)
                {
                  fprintf(stderr, "error: The argument to --rpc-password is longer than %d characters.\n Either specify a shorter password, or change the source code.\n", BITCOINRPC_PARAM_MAXLEN);
                  return 1;
                }
              strncpy(o->pass, optarg, BITCOINRPC_PARAM_MAXLEN);
            }
          if (strncmp("rpc-address", long_options[option_index].name, 11) == 0)
            {
              if (strlen(optarg) > BITCOINRPC_PARAM_MAXLEN)
                {
                  fprintf(stderr, "error: The argument to --rpc-address is longer than %d characters.\n Either specify a shorter address, or change the source code.\n", BITCOINRPC_PARAM_MAXLEN);
                  return 1;
                }
              strncpy(o->addr, optarg, BITCOINRPC_PARAM_MAXLEN);
            }
          if (strncmp("rpc-port", long_options[option_index].name, 8) == 0)
            {
              o->port = (unsigned int)atoi(optarg);
              if (o->port > 65535 || o->port <= 0)
                {
                  fprintf(stderr, "error: The port number in --rpc-port is out of range.\n");
                  return 1;
                }
            }

          break;

        case 'h':
          print_help_page();
          exit(EXIT_SUCCESS);

        case 'V':
          fprintf(stderr, "%s-%s\n", PROGNAME, BITCOINRPC_VERSION);
          exit(EXIT_SUCCESS);

        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
          abort();
        }
    } /* while(1) */

  if (pass_flag == 0)
    {
      fprintf(stderr, "Please specify at least a password for RPC client.\n");
      exit(EXIT_FAILURE);
    }

  /* Print any remaining command line arguments (not options). */
  /*if (optind < argc)
     {
     printf ("non-option ARGV-elements: ");
     while (optind < argc)
      printf ("%s ", argv[optind++]);
     putchar ('\n');
     }
   */
  return 0;
}




/* include all tests and parameters */
static char * all_tests(cmdline_options_t o)
{
  BITCOINRPC_RUN_TEST(global, o, NULL);
  BITCOINRPC_RUN_TEST(client, o, NULL);
  BITCOINRPC_RUN_TEST(method, o, NULL);
  BITCOINRPC_RUN_TEST(resp, o, NULL);
  BITCOINRPC_RUN_TEST(call, o, NULL);
  BITCOINRPC_RUN_TEST(calln, o, NULL);
  return 0;
}

int tests_run = 0;


int
main(int argc, char **argv)
{
  /* Parse command line options */
  cmdline_options_t o;

  /* defaults */
  strncpy(o.user, BITCOINRPC_USER_DEFAULT, BITCOINRPC_PARAM_MAXLEN);
  strncpy(o.pass, BITCOINRPC_PASS_DEFAULT, BITCOINRPC_PARAM_MAXLEN);
  strncpy(o.addr, BITCOINRPC_ADDR_DEFAULT, BITCOINRPC_PARAM_MAXLEN);
  o.port = BITCOINRPC_PORT_DEFAULT;


  if (parse_command_options(argc, argv, &o) != 0)
    {
      fprintf(stderr, "There was a problem parsing command line options.\nExit.\n");
      exit(EXIT_FAILURE);
    }

  fprintf(stdout, "%s-%s starting...\n", PROGNAME, BITCOINRPC_VERSION);

  if (bitcoinrpc_global_init() != BITCOINRPCE_OK)
    {
      fprintf(stderr, "error: cannot initialise the global state of the library\n");
      abort();
    }


  /* Testing suites here */
  fprintf(stdout, ">>> JSON data starts here:\n");
  fprintf(stderr, "[");
  fprintf(stderr, "{\"test\": \"_internal_\",  \"result\": true, \"id\": %d}", tests_run++);

  char *result = all_tests(o);

  fprintf(stderr, "]\n");
  fprintf(stdout, "<<< JSON data ends here.\n");

  if (result != 0)
    {
      fprintf(stdout, "TEST FAILED\n");
    }
  else
    {
      fprintf(stdout, "ALL TESTS PASSED\n");
    }
  fprintf(stdout, "Tests run: %d\n", tests_run);

  fprintf(stdout, "Free the resources... \n");
  if (bitcoinrpc_global_cleanup() != BITCOINRPCE_OK)
    {
      fprintf(stderr, "failure.\n");
      abort();
    }
  fprintf(stdout, "done.\n");

  return result != 0;
}
