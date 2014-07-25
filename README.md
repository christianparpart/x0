# Xzero HTTP Framework

[ ![Build status - Travis-ci](https://secure.travis-ci.org/xzero/libxzero.png) ](http://travis-ci.org/xzero/libxzero)

- official website: http://xzero.io
- github: http://github.com/xzero/libxzero
- travis-ci: https://travis-ci.org/xzero/libxzero

`libxzero` is a thin low-latency and scalable HTTP server framework
written in modern C++.

## Framework Features

- Thin and clean core API
- Response output filter API
- HTTP/1.1, including pipelining
- fully asynchronous response content generation support
- zero-copy networking optimization through sendfile() system call
- transmitting of static files with partial response support and cache-friendly

# INSTALLATION REQUIREMENTS:

- libbase from https://github.com/xzero/libbase.git
- gcc >= 4.8.0 (for building only, CLANG >= 3.4 is also supported)
- cmake (for building only)
- zlib (optional & recommended, for compression)
- bzip2 (optional & recommended, for compression)
- gtest (optional, for unit testing)

### Building from Source on Ubuntu 14.04:

    # Installs required dependencies
    sudo apt-get install make cmake gcc-4.8 g++-4.8 libgnutls28-dev libgcrypt11-dev \
        libmysqlclient-dev libev-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev libtbb-dev libpam-dev git
    
    # If you want to built the tests, you must install libgtest-dev and then built it yourself
    sudo apt-get install libgtest-dev
    cd /usr/src/gtest && sudo cmake . && sudo make && sudo cp -vpi libgtest*.a /usr/local/lib/; cd -
    
    # Install git and clone repository
    git clone git://github.com/xzero/x0.git && cd x0
    
    # Now run cmake to bootstrap the build
    cmake -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX=$HOME/local
    
    # Ensure installation target prefix
    mkdir $HOME/local
    
    # Now compiling should just work.
    make && make install
    
    # Run web server on port 8080
    `pwd`/x0d/src/x0d --instant=`pwd`/www/htdocs,8080
    
    # or try its CLI help
    `pwd`/x0d/src/x0d -h

    # have fun hacking and don't forget to checkout the just installed man pages ;-)

### Build from Sources on Ubuntu 12.04

    # Ensure required GCC (version 4.8)
    sudo apt-get install python-software-properties
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt-get update
    sudo apt-get install gcc-4.8 g++-4.8

    # Installs required dependencies
    sudo apt-get install make cmake pkg-config git
    
    # If you want to built the tests, you must install libgtest-dev and then built it yourself
    sudo apt-get install libgtest-dev
    cd /usr/src/gtest && sudo cmake -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX . && \
        sudo make && sudo cp -vpi libgtest*.a /usr/local/lib/; cd -
    
    # Install git and clone repository
    git clone git://github.com/xzero/libxzero.git
    cd libxzero
    git subtree add -P 3rdparty/libbase git://github.com/xzero/libbase.git
    
    # Now run cmake to bootstrap the build
    cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++-4.8 .
    
    # Now compiling should just work.
    make && sudo make install
    
    # have fun hacking ;-)

LICENSE
-------

```
Copyright (c) 2009-2014 Christian Parpart <trapni@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
