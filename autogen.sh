#! /bin/bash

if [[ -z "$CXX" ]]; then
  if which clang++ &>/dev/null; then
    export CXX=$(which clang++)
  else
    export CXX=$(which g++)
  fi
  if which clang &>/dev/null; then
    export CC=$(which clang)
  else
    export CC=$(which cc)
  fi
fi

#CXXFLAGS="-I/usr/local/include"

cmake "$(dirname $0)" \
  -DX0D_CONFIGFILE="`dirname $0`/x0d.conf" \
  -DX0D_USER="`whoami`" \
  -DX0D_GROUP="`groups | cut -d' ' -f1`" \
  -DCMAKE_BUILD_TYPE="debug" \
  -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3" \
  -DCMAKE_CXX_COMPILER="${CXX}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_C_COMPILER="${CC}" \
  -DCMAKE_INSTALL_PREFIX="${HOME}/local" \
  -DOPENSSL_INCLUDE_DIR="/usr/local/opt/openssl/include" \
  -DOPENSSL_SSL_LIBRARY="/usr/local/opt/openssl/lib/libssl.dylib"
