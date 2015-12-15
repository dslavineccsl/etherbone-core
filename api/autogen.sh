#! /bin/sh
cd $(dirname $(readlink -e "$0"))
set -ex
aclocal -I auto
autoheader
autoconf
libtoolize -c
automake -a -c
