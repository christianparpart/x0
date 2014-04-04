# Xzero (x0) HTTP Web Server and Framework

[ ![Build status - Travis-ci](https://secure.travis-ci.org/xzero/x0.png) ](http://travis-ci.org/xzero/x0)

- official website: http://xzero.io/ (work in progress)
- github: http://github.com/xzero/x0
- ohloh: http://www.ohloh.net/p/x0
- travis-ci: https://travis-ci.org/xzero/x0

x0 is a thin low-latency and scalable HTTP web server and web service framework
written in modern C++.

# FEATURES

- HTTP/1.1, including pipelining
- thin and clean core API with powerful plugin system
- fully asynchronous response content generation support
- response output filter API
- zero-copy networking optimization through sendfile() system call
- transmitting of static files with partial response support and cache-friendly
- instant mode (configuration-less basic HTTP serving)
- flow-based configuration system, just-in-time compiled into native CPU instructions
- automatic directory indexing
- customizable error pages
- on-the-fly executable upgrade with gracefully finishing currently active requests.
- CGI/1.1 support (plugin)
- FastCGI support (plugin)
- HTTP reverse proxy support (plugin)
- name based virtual hosting (plugin)
- SSL connection encryption (plugin)
  - SSL Server Name Indication (SNI) extension
- dynamic content compression (plugin)
- basic authentication (plugin)
- advanced dynamic load balancer (plugin)
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
- request path aliasing (plugin)
- automatic directory listing generation (plugin)
- apache-style access log (plugin)
- user-directory support (plugin)
- browser match support (plugin)
- customized Expires and Cache-Control response header control (plugin)

# INSTALLATION REQUIREMENTS:

- gcc >= 4.8.0 (for building only, CLANG >= 3.4 is also supported)
- libev >= 4.0
- cmake (for building only)
- tbb, Threading Building Blocks (required)
- zlib (optional & recommended, for compression)
- bzip2 (optional & recommended, for compression)
- gnutls (optional & recommended, for SSL/TLS encryption)
- gtest (optional, for unit testing)

## Building from Source on Ubuntu 14.04:

    # Installs required dependencies
    sudo apt-get install make cmake gcc-4.8 g++-4.8 libgnutls28-dev libgcrypt11-dev \
        libmysqlclient-dev libev-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools0 libtbb-dev libpam-dev git
    
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

## Build from Sources on Ubuntu 12.04

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

