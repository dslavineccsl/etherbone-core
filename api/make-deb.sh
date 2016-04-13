#! /bin/sh
JOBS=${JOBS:-4} # run 4 commands at once

cd $(dirname $(readlink -e "$0"))
set -ex
./autogen.sh
./configure --enable-maintainer-mode
make -j $JOBS distcheck

tarball=$(echo etherbone-*.tar.gz)
ver=${tarball##*-}
ver=${ver%%.tar.gz}
orig=etherbone_$ver.orig.tar.gz

rm -rf deb
mkdir deb
mv $tarball deb/$orig
cd deb
tar xvzf $orig
cp -a ../debian etherbone-${ver}/
cd etherbone-${ver}
debuild -eDEB_BUILD_OPTIONS="parallel=$JOBS" 
