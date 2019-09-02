# CashWeb: libraries + executables

C libraries and useful executables for sending/getting from the Bitcoin Cash blockchain under the CashWeb protocol.


### Build dependencies

These dependencies are required:

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  mongoc     | querying MongoDB directly    | tested with libmongoc-dev-1.9.2+dfsg-1build1 on Ubuntu; mongo-c-driver-1.14.0 on macOS
  curl       | querying BitDB HTTP endpoint | tested with libcurl4-openssl-dev-7.58.0-2ubuntu3.7 on Ubuntu; curl-7.65.3 on macOS
  jansson    | JSON parsing/creation        | tested with libjansson-dev-2.11-1 on Ubuntu; jansson-2.12 on macOS

Required for cashsendtools/cashsend:

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  uuid       | generate UUIDs               | tested with uuid-dev-2.31.1-0.4ubuntu3.3 on Ubuntu; ossp-uuid-1.6.2 on macOS

Required for cashserver:

 Library     | Purpose                      | Description
 ------------|------------------------------|----------------------------------------------------------------------------------------
  microhttpd | basic HTTP server functions  | tested with libmicrohttpd-dev-0.9.59-1 on Ubuntu; libmicrohttpd-0.9.63 on macOS

To install the build dependencies on Ubuntu, enter the following command:

    sudo apt-get install libmongoc-dev libcurl4-openssl-dev libjansson-dev uuid-dev libmicrohttpd-dev


## Build/install

Please make sure that you have all the required dependencies installed.<br/>
Then type in the project folder:

    ./autogen.sh && ./configure

NOTE: Append flag --without-cashsend to omit the cashsendtools library + cashsend executable.<br/>
      Append flag --without-cashserver to omit the cashserver executable.

Then type in:
	
    make && make install

NOTE: 'make install' may require sudo privileges.<br/>
This will build the libraries + executables and install to your system.

If you want to clean up compiled files and start from scratch:

    make distclean

To uninstall at any time:

    make uninstall


## Usage

Available executables for experimentation with getting, sending, and serving (respectively):

    cashget [FLAGS] <toget>

    cashsend [FLAGS] <tosend>

    cashserver [FLAGS]

To use a library, it is enough to include the header file:

    #include <cashgettools.h>

and/or

    #include <cashsendtools.h>

in your source code and provide the following linker flag(s) during compilation:

    -lcashgettools

and/or

    -lcashsendtools

For further information, see the header file(s): [`src/cashgettools.h`](./src/cashgettools.h), [`src/cashsendtools.h`](./src/cashsendtools.h).<br/>
Dedicated documentation is not yet available.

*Please notice that the code is in the very early stage of development; highly experimental.*


## License

The source code is released under the terms of the MIT license.  Please, see
[LICENSE](./LICENSE) for more information.


*last updated: 2019-09-01*
