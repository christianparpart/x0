#! /bin/bash

# osx
cmake .. \
    -GNinja \
    -DCMAKE_BUILD_TYPE=debug \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DOPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include/ \
    -DOPENSSL_SSL_LIBRARY=/usr/local/opt/openssl/lib/libssl.dylib
