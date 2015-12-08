#! /bin/sh
cd $(dirname $(readlink -e "$0"))
aclocal -I auto
autoheader
autoconf
libtoolize -c
automake -a -c
