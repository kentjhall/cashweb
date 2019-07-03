# The MIT License (MIT)
# Copyright (c) 2016 Andrea Bernardo Ciddio (Code::Chunks) and Marek Miller
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

MAJOR := 0
MINOR := 2
VERSION := $(MAJOR).$(MINOR)

NAME        := bitcoinrpc
SRCDIR      := src
DOCDIR      := doc
TESTNAME    := $(NAME)_test
TESTDIR 	  := test
LIBDIR      := .lib
BINDIR      := bin
LDFLAGS     := -luuid -ljansson -lcurl
TESTLDFLAGS := -ljansson -lm

CFLAGS := -fPIC -O3 -g -Wall -Werror -Wextra -std=c99
TESTCFLAGS =

CC := gcc

SHELL := /bin/sh

INSTALL_PREFIX     ?= 		# leave empty for /

INSTALL_DOCSPATH   := $(INSTALL_PREFIX)/usr/share/doc
INSTALL_LIBPATH    := $(INSTALL_PREFIX)/usr/local/lib
INSTALL_HEADERPATH := $(INSTALL_PREFIX)/usr/local/include
INSTALL_MANPATH    := $(INSTALL_PREFIX)$(shell manpath | cut -f 1 -d ":")

# Programs for installation
INSTALL_PROGRAM = $(INSTALL)
INSTALL = install
INSTALL_DATA = $(INSTALL) -m 644

RM = rm -fv

# -----------------------------------------------------------------------------
CFLAGS += -D VERSION=\"$(VERSION)\"

SRCFILES = $(shell find $(SRCDIR) -maxdepth 1 -iname '*.c')
OBJFILES = $(shell echo $(SRCFILES) | sed 's/\.c/\.o/g')

.PHONY: all
all: prep lib

.PHONY: prep
prep:
	@echo
	@mkdir -p $(LIBDIR)


.PHONY: lib
lib: $(LIBDIR)/lib$(NAME).so

$(LIBDIR)/lib$(NAME).so: $(LIBDIR)/lib$(NAME).so.$(VERSION)
	ldconfig -v -n $(LIBDIR)
	ln -fs lib$(NAME).so.$(MAJOR) $(LIBDIR)/lib$(NAME).so

$(LIBDIR)/lib$(NAME).so.$(VERSION): $(OBJFILES)
	$(CC) $(CFLAGS) -shared -Wl,-soname,lib$(NAME).so.$(MAJOR) \
	$(OBJFILES) \
	-o $@ \
	-Wl,--copy-dt-needed-entries $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -o $@ -c $<


# --------- test -----------------
BITCOINDPASS    := libbitcoinrpc-test
BITCOINDATADIR  := /tmp/libbitcoinrpc-test

BITCOINBINPATH  ?= /usr/local/bin
BITCOIND         = $(BITCOINBINPATH)/bitcoind
BITCOINCLI       = $(BITCOINBINPATH)/bitcoin-cli
BITCOINPARAMS    = -regtest -datadir=$(BITCOINDATADIR) -rpcpassword=$(BITCOINDPASS)

BITCOIN_VERSION_HEX ?= 0xffffff
TESTCFLAGS += -D BITCOIN_VERSION_HEX=$(BITCOIN_VERSION_HEX)

TESTSRCFILES = $(shell find $(TESTDIR) -maxdepth 1 -iname '*.c')
TESTOBJFILES = $(shell echo $(TESTSRCFILES) | sed 's/\.c/\.o/g')

.PHONY: build-test
build-test: $(TESTDIR)/$(TESTNAME)

$(TESTDIR)/$(TESTNAME): $(TESTOBJFILES)
	$(CC) $(CFLAGS) $(TESTCFLAGS) $(TESTOBJFILES) -o $@ \
		-l$(NAME) $(TESTLDFLAGS) -L$(LIBDIR) -Wl,-rpath=$(LIBDIR)

$(TESTDIR)/%.o: $(TESTDIR)/%.c
	$(CC) $(CFLAGS) $(TESTCFLAGS) -c $< -o $@ \
		-l$(NAME) -L$(LIBDIR) -I $(SRCDIR) -Wl,-rpath=$(LIBDIR)


.PHONY: prep-test
prep-test:
	@echo "Testing library routines in regtest mode (bitcoind and bitcoin-cli needed!)"
	@if ! which $(BITCOIND) ; then echo "Can't find bitcoind executable in $(BITCOINBINPATH). Try setting BITCOINBINPATH." ; exit 1; fi
	@if ! which $(BITCOINCLI) ; then echo "Can't find bitcoin-cli executable $(BITCOINBINPATH). Try setting BITCOINBINPATH." ; exit 1; fi
	@echo "Prepare regtest blockchain"
	@echo "datadir: $(BITCOINDATADIR)"
	@mkdir -p $(BITCOINDATADIR)
	$(BITCOIND) -version || true
	$(BITCOIND) -daemon $(BITCOINPARAMS)
	@sleep 3s;
	

.PHONY: preform-test
perform-test:
	@echo "Start $(TESTNAME)"
	$(DEBUGGER) $(TESTDIR)/$(TESTNAME) --rpc-password=$(BITCOINDPASS) --rpc-port=18332


.PHONY: clean-test
clean-test:
	@$(BITCOINCLI) $(BITCOINPARAMS) stop 2> /dev/null || true # server is probably alredy stoped by test programm
	sleep 5s;
	@ #echo "Bitcoin Core logs:"
	@ #cat $(BITCOINDATADIR)/regtest/debug.log
	@echo "Remove datadir"
	rm -rf $(BITCOINDATADIR)


.PHONY: test
test: all build-test prep-test perform-test clean-test


# ---------- clean ----------------
.PHONY: clean
clean:
	$(RM) ./*.o $(SRCDIR)/*.o $(TESTDIR)/*.o
	$(RM) ./*.gch $(SRCDIR)/*.gch $(TESTDIR)/*.gch
	$(RM) $(TESTDIR)/$(TESTNAME)
	$(RM) $(LIBDIR)/*.so*
	$(RM) -d $(LIBDIR) $(BINDIR)


# ---------- install --------------
.PHONY: install
install:
	@echo "Installing to $(INSTALL_PREFIX)"
	@mkdir -p $(INSTALL_PREFIX) $(INSTALL_LIBPATH) $(INSTALL_DOCSPATH) $(INSTALL_MANPATH) $(INSTALL_HEADERPATH)
	$(INSTALL) $(LIBDIR)/lib$(NAME).so.$(VERSION) $(INSTALL_LIBPATH)
	ldconfig  -n $(INSTALL_LIBPATH)
	ln -fs lib$(NAME).so.$(MAJOR) $(INSTALL_LIBPATH)/lib$(NAME).so
	$(INSTALL_DATA) $(SRCDIR)/$(NAME).h $(INSTALL_HEADERPATH)
	@echo "Installing docs to $(INSTALL_DOCSPATH)/$(NAME)"
	mkdir -p $(INSTALL_DOCSPATH)/$(NAME)
	$(INSTALL_DATA) $(DOCDIR)/*.md $(INSTALL_DOCSPATH)/$(NAME)
	$(INSTALL_DATA) CREDITS $(INSTALL_DOCSPATH)/$(NAME)
	$(INSTALL_DATA) LICENSE $(INSTALL_DOCSPATH)/$(NAME)
	$(INSTALL_DATA) Changelog.md $(INSTALL_DOCSPATH)/$(NAME)
	@echo "Installing man pages"
	@mkdir -p $(INSTALL_MANPATH)/man3
	$(INSTALL_DATA) $(DOCDIR)/man3/$(NAME)*.gz $(INSTALL_MANPATH)/man3

.PHONY: uninstall
uninstall:
	@echo "Removing from $(INSTALL_LIBPATH)/"
	@$(RM) $(INSTALL_LIBPATH)/lib$(NAME).so
	@$(RM) $(INSTALL_LIBPATH)/lib$(NAME).so.$(MAJOR)
	@$(RM) $(INSTALL_LIBPATH)/lib$(NAME).so.$(MAJOR).$(MINOR)
	ldconfig  -n $(INSTALL_LIBPATH)
	@$(RM) $(INSTALL_HEADERPATH)/$(NAME).h
	@$(RM) $(INSTALL_MANPATH)/man3/$(NAME)*.gz
