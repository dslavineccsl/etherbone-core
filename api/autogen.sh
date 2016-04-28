#! /bin/sh
cd $(dirname "$0")
set -ex
git log > ChangeLog
aclocal -I auto
autoheader
autoconf
which libtoolize >/dev/null && libtoolize -c || glibtoolize -c
automake -a -c
