#! /bin/bash

set -e

# compile times dependencies
CDEPENDS="
make cmake clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config
libpcre3-dev libfcgi-dev libgoogle-perftools-dev libtbb-dev
libpam-dev libgtest-dev ninja-build"

# runtime dependencies
RDEPENDS="libssl1.0.0 zlib1g libbz2-1.0 libpcre3 libtbb2 libpam0g"

# Installs required dependencies
apt-get install -qqy $CDEPENDS $RDEPENDS

# googletest framework
cd /usr/src/gtest && cmake . && make && \
    cp -vpi libgtest*.a /usr/local/lib/; cd -

# cmake to bootstrap the build
cd /usr/src/x0
cmake -GNinja \
      -DCMAKE_BUILD_TYPE=release \
      -DCMAKE_C_COMPILER=/usr/bin/clang-3.5 \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++-3.5 \
      -DX0D_CLUSTERDIR=/var/lib/x0d \
      -DX0D_LOGDIR=/var/log/x0d \
      -DX0D_TMPDIR=/tmp

# compiling should just work.
ninja

# install
mkdir -p /etc/x0d /var/log/x0d /var/lib/x0d /var/www
cp src/x0d/x0d /usr/bin/x0d
cp src/x0d/x0d.conf-dist /etc/x0d/x0d.conf

# cleanup
apt-get purge -y $CDEPENDS
apt-get autoremove -y
rm -rf /usr/src
