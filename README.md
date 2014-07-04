# Xzero HTTP Web Server & Framework

[ ![Build status - Travis-ci](https://secure.travis-ci.org/xzero/x0.png) ](http://travis-ci.org/xzero/x0)

- official website: http://xzero.io
- github: http://github.com/xzero/x0
- ohloh: http://www.ohloh.net/p/x0
- travis-ci: https://travis-ci.org/xzero/x0

x0 is a thin low-latency and scalable HTTP web server and web service framework
written in modern C++.

## Framework Features

- Thin and clean core API
- response output filter API
- HTTP/1.1, including pipelining
- intuitive configuration language
- fully asynchronous response content generation support
- zero-copy networking optimization through sendfile() system call
- transmitting of static files with partial response support and cache-friendly

# x0d, Dedicated HTTP Application Gatway Server

`x0d` is the dedicated HTTP web server that is built ontop of the
Xzero Framework.

It supports a very expressive and natural configuration language and
a number of standard plugins to become *your* web application gateway.

### x0d Features

- automatic directory indexing
- customizable error pages
- on-the-fly executable upgrade with gracefully finishing currently active requests.
- extensible via the powerful plugin system
- CGI/1.1 support
- SSL connection encryption
  - SSL Server Name Indication (SNI) extension
- dynamic content compression
- basic authentication
- advanced dynamic load balancer
  - supporting different backend transports (TCP/IP, and UNIX Domain Sockets).
  - supporting different backend protocols (HTTP and FastCGI)
  - supporting different backend acceleration strategies (memory, disk)
  - advanced health monitoring
  - JSON API for retrieving state, stats,
    and reconfiguring clusters (including adding/updating/removing backends).
  - client side routing support
  - sticky offline mode
  - X-Sendfile support, full static file transmission acceleration
  - HAproxy compatible CSV output
  - Object Cache
    - fully runtime configurable
    - client-cache aware
    - life JSON stats
    - supports serving stale objects when no backend is available
    - Vary-response support.
    - multiple update strategies (locked, shadowed)
- request path aliasing
- automatic directory listing generation
- apache-style access log
- user-directory support
- browser match support
- customized Expires and Cache-Control response header control
- instant mode (configuration-less basic HTTP serving)

# INSTALLATION REQUIREMENTS:

- gcc >= 4.8.0 (for building only, CLANG >= 3.4 is also supported)
- libev >= 4.0
- cmake (for building only)
- tbb, Threading Building Blocks (required)
- zlib (optional & recommended, for compression)
- bzip2 (optional & recommended, for compression)
- gnutls (optional & recommended, for SSL/TLS encryption)
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
    sudo apt-get install make cmake libgnutls28-dev libgcrypt11-dev \
        libmysqlclient-dev libev-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools0 libtbb-dev libpam-dev git
    
    # If you want to built the tests, you must install libgtest-dev and then built it yourself
    sudo apt-get install libgtest-dev
    cd /usr/src/gtest && sudo cmake -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX . && \
        sudo make && sudo cp -vpi libgtest*.a /usr/local/lib/; cd -
    
    # Install git and clone repository
    git clone git://github.com/xzero/x0.git && cd x0
    
    # Now run cmake to bootstrap the build
    cmake -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX=$HOME/local \
          -DCMAKE_C_COMPILER=/usr/bin/gcc-4.8 -DCMAKE_CXX_COMPILER=/usr/bin/g++-4.8 .
    
    # Ensure installation target prefix
    mkdir $HOME/local
    
    # Now compiling should just work.
    make && make install
    
    # Run web server on port 8080
    `pwd`/x0d/src/x0d --instant=`pwd`/www/htdocs,8080
    
    # or try its CLI help
    `pwd`/x0d/src/x0d -h

    # have fun hacking and don't forget to checkout the just installed man pages ;-)

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
