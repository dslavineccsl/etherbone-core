#! /bin/sh
aclocal -I auto
autoheader
autoconf
libtoolize -c
automake -a -c
