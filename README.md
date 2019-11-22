# CashWeb: libraries + executables (+ WebAssembly JS!)

C libraries and useful executables for sending/getting from the Bitcoin Cash blockchain under the CashWeb protocol.


## Build dependencies

Autotools (`autoconf`, `automake`, and `libtool`) are required for building.

These project dependencies are required:

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  curl       | querying HTTP endpoint       | tested with libcurl4-openssl-dev-7.58.0-2ubuntu3.7 on Ubuntu; curl-7.65.3 on macOS
  jansson    | JSON parsing/creation        | tested with libjansson-dev-2.11-1 on Ubuntu; jansson-2.12 on macOS

Required if optionally building cashgettools to query local BitDB-populated MongoDB (and cashget/cashserver by extension):

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  mongoc     | querying MongoDB directly    | tested with libmongoc-dev-1.9.2+dfsg-1build1 on Ubuntu; mongo-c-driver-1.14.0 on macOS

Required if optionally building cashsendtools (and cashsend by extension):

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  uuid       | generate UUIDs               | tested with uuid-dev-2.31.1-0.4ubuntu3.3 on Ubuntu; ossp-uuid-1.6.2 on macOS

Required if optionally building cashserver:

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  microhttpd | basic HTTP server functions  | tested with libmicrohttpd-dev-0.9.59-1 on Ubuntu; libmicrohttpd-0.9.63 on macOS

To install all build dependencies on Ubuntu:

    sudo apt-get install autoconf automake libtool libmongoc-dev libcurl4-openssl-dev libjansson-dev uuid-dev libmicrohttpd-dev

And on macOS (with Homebrew):

    brew install autoconf automake libtool curl jansson mongo-c-driver ossp-uuid libmicrohttpd


## Build/Install

First clone this repository

Please make sure that you have all the required dependencies installed.<br/>
Then configure with the following commands:

    ./autogen.sh
    ./configure

**NOTE:** append configure flag `--without-mongodb` to omit cashgettools MongoDB querying functionality, `--without-cashsend` to omit the cashsendtools library + cashsend executable, and/or `--without-cashserver` to omit the cashserver executable.

And then make:
	
    make && make install

**NOTE:** `make install` may require sudo privileges.<br/>
This will build the libraries + executables and install to your system.

To uninstall at any time:

    make uninstall

To clean up compiled files and start from scratch:

    make distclean


## Usage

Available executables for experimentation with getting, sending, and serving (respectively):

    cashget [FLAGS] <toget>

    cashsend [FLAGS] <tosend>

    cashserver [FLAGS]

**NOTE:** use flag `-h` for usage details

To use a library, it is enough to include the header file:

    #include <cashgettools.h>

and/or

    #include <cashsendtools.h>

in your source code and provide the following linker flag(s) during compilation:

    -lcashgettools

and/or

    -lcashsendtools

along with

    -ljansson -lcurl

and if using MongoDB querying, followed by

    $(pkg-config --libs libmongoc-1.0)

For further information, see the header file(s): [`src/cashgettools.h`](./src/cashgettools.h), [`src/cashsendtools.h`](./src/cashsendtools.h).<br/>
Dedicated documentation is not yet available.

**NOTE:** cashsendtools/cashsend works over RPC in current implementation, so full-node software is required for testing; however, all necessary RPC calls should function with pruning enabled, so that is an option if storage is a concern.

*Please notice that this project is in a very early stage of development; highly experimental.*


## Build to Javascript (WebAssembly)

This probably isn't necessary to do yourself; the latest build should always be available at [the Browser Buddy repo](https://github.com/kentjhall/cashweb-bb) in the form of `cashgettools_wasm.js`, `cashgettools_wasm.wasm`, and `cashgettools_wasm.data`. (Notice that only cashgettools is available for now; I do plan to work on cashsendtools, but this will be much less straightforward.) I don't believe that building on your own system would offer much benefit (given that we're compiling to Javascript), but if you would like to tinker around with it, the process is relatively straightforward.

Install the Emscripten SDK as per [these instructions](https://emscripten.org/docs/getting_started/downloads.html) with the *upstream* backend (should be the default now).<br>
Then configure as follows:

    ./autogen.sh
    emconfigure ./configure --enable-emscripten

And make normally:

    make

This will build `cashgettools_wasm.js`, `cashgettools_wasm.wasm`, and `cashgettools_wasm.data` in the `src` directory; they can be copied into a Javascript project from there. For an implementation example, see the browser extension repository referenced above.


## Addendum

Proceed with caution, particularly when using cashsendtools/cashsend–**I do not recommend risking any significant BCH on this for the time being**. Again, this is all highly experimental.

cashgettools/cashget/cashserver seem to be running valgrind-clean for now, but this requires much more testing.

cashsendtools/cashsend are currently untested valgrind-wise; TODO.


## Protocol Details


### Disclaimer

While I will speak definitively on the protocol itself, there is bound to be unexpected behavior in making use of the libraries/executables detailed below–this software is criminally undertested, so please let me know if you find a query/result doesn't behave as described below.


### Overview

A CashWeb file is composed of any number of transactions on the blockchain that reference each other in their OP\_RETURN data.
These references may be structured as a chain, a tree, or some combination of the two. However the TXs are structured,
there will always be a root/starting TX whose TXID serves as an identifier for the file. The OP\_RETURN data of this root TX
must be suffixed with 12 bytes of CashWeb metadata—4 for the chain length, 4 for the tree depth, 2 for the CashWeb type value,
and 2 for the CashWeb protocol version. Below is a valid root TX's first output:

    OP_RETURN 72750f1ccc3efe9f10e451f0a5967f0168d722a297dd69abb304aa64430fcdd865f091c1d640c9db18390dad 00000003 00000003 02e2 0000 

This file has a length of 3, depth of 3, CashWeb type value of 738 (this corresponds to MIME type video/mp4 under the current protocol),
and CashWeb protocol version 0. The last 32 bytes of non-metadata data is interpreted as the TXID of the next TX in the chain,
and any remaining data is the top TXID(s) of the tree (may or may not be partial; if partial, will be completed going down the chain).
Chained files are typically slower to load and more expensive to send, but allow for loading in chunks;
this particular example happens to house a two second video encoded for progressive loading.


### Nametags/Scripting

Any file queried by TXID is completely immutable; i.e., querying the same TXID will always yield the same data.
This is not the case for a Nametag ID, whose content is mutable (supports revisioning) and easily queryable.
A valid CashWeb Nametag ID might be *~coolcashwebname*; simply the name (*coolcashwebname*) prefixed with a tilde.
Anyone can claim a name at any time, but only one claim will be recognized under the CashWeb protocol;
first and foremost will be the claim(s) contained within the earliest block, and between those (when multiple),
it will be a toss-up as to whose claim TXID is first when sorted in lexicographic order (so it's random, but definitive).
If a claim's data is not encoded as a CashWeb file, it is dismissed for the next one (presumed accidental),
but this is the only exception; if a claim or revision's script is invalid,
the interpreter should simply look for the next revision, but never the next claim.

All Nametag claims and revisions must contain a CashWeb script to be considered valid;
scripting is handled similarly to any simple stack-based language, including Bitcoin's own.
All current codes are listed in [`src/cashwebuni.h`](./src/cashwebuni.h); most are fairly self-explanatory.
The important code for now is *CW_OP_NEXTREV*, which essentially tells the interpreter to check for the next revision;
if there, execute it's script before proceeding, and if not, just continue. If a script lacks this code,
it is completely immutable (although not necessarily it's data content, as the script may reference a still-mutable Nametag).
Below is an example of a valid Nametag claim's first output:

    OP_RETURN fefd33c05e3d43f7301c4b69317ff578e9a558e6027bf3712b129c5781f8bb07864dfc000000000000000000000000 7e636f6f6c636173687765626e616d65

The first pushdata contains the script, encoded as any CashWeb file (no limit to it's length; script may span multiple TXs),
and the second pushdata must always be the claimed ID; if run through a Hex-to-ASCII converter,
you'll see this example resolves to *~coolcashwebname*. Let's examine the script:

    CW_OP_NEXTREV CW_OP_PUSHTXID 33c05e3d43f7301c4b69317ff578e9a558e6027bf3712b129c5781f8bb07864d CW_OP_WRITEFROMTXID

This is what a standard Nametag claim might look like; execute the next revision's script (if there), then push the TXID to stack,
and write from the data stored there (presuming it's a valid CashWeb file; should report a script error if not).
This simple claim will effectively "name" the file at TXID *33c05e3d43f7301c4b69317ff578e9a558e6027bf3712b129c5781f8bb07864d* as *coolcashwebname*;
the identifier *~coolcashwebname* will continue to point to this file until the owner makes an update to the script.
This brings us to revisioning–when the claim is sent, there should be a **change output created at VOUT 1–whereas data is at VOUT 0–to be used as the first input for a future revision's root TX**.
Essentially, whoever holds the private key that unlocks this UTXO controls the Nametag;
you may send this tiny unspent to someone else to transfer ownership, for example. Let's look at what the revision's output data might look like:

    OP_RETURN fefded2cf5437666d8fd84f11cf615c7f51bc0e4f8bb8524d402b67a3c07168af41afcff000000000000000000000000

As you can see, it looks awfully similar to the original claim, except it lacks the second pushdata; of course,
this is because this TX will be queried by its first input's TXID/VOUT, rather than by Nametag ID directly.
Let's take a closer look at the script:

    CW_OP_NEXTREV CW_OP_PUSHTXID ed2cf5437666d8fd84f11cf615c7f51bc0e4f8bb8524d402b67a3c07168af41a CW_OP_WRITEFROMTXID CW_OP_TERM

Again, very similar in form, save for the code *CW\_OP\_TERM* at the end; this code is very important, as it tells the interpreter to stop prematurely.
This script acts as a simple replace; rather than point to the original file, CashWeb ID *~coolcashwebname* now points to the file
at TXID *ed2cf5437666d8fd84f11cf615c7f51bc0e4f8bb8524d402b67a3c07168af41a*. Without our *CW\_OP\_TERM* suffixing the script,
it would continue executing through the end of our initial claim's script, acting as a prepend rather than a replace
(the script would first write from the new TXID, and then proceed to write from the original).

There is another type of CashWeb ID–distinct from the TXID and Nametag ID–which is the Nametag Version ID.
If we were to query the Nametag ID *~coolcashwebname*, we should get the latest revision; however, if an earlier revision is sought
(remember, all data on the blockchain is permanent!–there's no actual "replacing", just redirecting), you may, for example,
query for *0~coolcashwebname* to get its first version, or *1~coolcashwebname* to get its second, or so on.
It should be noted, when querying for an existing version of a Nametag, you can be certain that its script is immutable
(this query will always execute the same script); however, if this script references another Nametag, its content is not necessarily immutable.


### Directory Indexes

Directory indexes are sent as any other file, but with a dedicated CashWeb type value in the metadata (*CW\_T\_DIR* – see [`src/cashwebuni.h`](./src/cashwebuni.h)).
A directory index's formatting is relatively simple; paths (all beginning with '/') and valid CashWeb IDs that are delimited by newlines ('\n').
An ID will directly follow its corresponding path, except when that ID is a TXID; all TXIDs are appended as byte data to the end of the directory index,
in the order in which they are found, after a terminating empty line. This serves to conserve space, as storing the doubly long hex strings
would be unnecessarily expensive. 

It should be noted that other directories (subdirectories, in effect) can be referenced within the directory
by valid CashWeb ID, but its path must be appended with '/'. These subdirectories should function as you might expect with regard to path interpretation,
but will of course require extra, potentially unnecessary requests. Also, directories may contain 'path links' to be used instead of the CashWeb ID (formatting-wise)
to effectively redirect from one path to another  following within the same directory. The linked path must be prepended with '.';
note that path links may be prepended to a directory index by Nametag scripting, so that a path may be redirected differently
when referenced from different Nametags.
The following is a valid CashWeb directory index that incorporates all these elements, save for TXIDs (those don't print well):

    /
    ./hello
    /saffron/
    ~saffron
    /hell
    ~inserttest
    /hello
    ~appendtest
    /saffron.html
    ~saffron/saffron.html
    (empty line)

It can also be noted that querying for path "/" should have the default behavior of serving the raw directory index if not specified;
in this case, however, a query for this path would be equivalent to a query for path "/hello", which in turn queries for CashWeb ID *~appendtest*.

So how do we query a path in a directory? This can be done with the fourth and final type of CashWeb ID–the Path ID. A Path ID may begin with any of the three previously mentioned ID types–TXID, Nametag ID, or Nametag Version ID–and is followed by a '/'-prefixed path. An example of this can already be seen in the above directory index, with the ID *~saffron/saffron.html*; in this way, it is perfectly legal for a directory to reference a file in another directory as its own. No distinction is made between a path that points to a full path stored within a directory index versus one that goes through a subdirectory; for example, if this directory were referenced under the identifier *~dirdir* (currently, it is) and you queried the Path ID *~dirdir/saffron/images/saffron.jpg*, there would be no way of knowing whether "/saffron/" is a separate subdirectory referenced from *~dirdir*, or if "/saffron/images/" is, or if "/images/" is a subdirectory of subdirectory "/saffron/", or if there are no subdirectories involved whatsoever and "/saffron/images/saffron.jpg" is simply a full path stored within *~dirdir*. Of course, this can always be checked by analyzing the directory index, but no distinction is made within the ID itself.


## License

The source code is released under the terms of the MIT license.  Please, see
[LICENSE](./LICENSE) for more information.


*last updated: 2019-09-01*
