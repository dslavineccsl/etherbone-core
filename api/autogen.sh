#! /bin/sh
cd $(dirname $(readlink -e "$0"))
set -ex
git log > ChangeLog
aclocal -I auto
autoheader
autoconf
libtoolize -c
automake -a -c
