#! /bin/sh

aclocal \
&& automake --add-missing \
&& autoconf \
&& cd src/jansson && ./autogen.sh
