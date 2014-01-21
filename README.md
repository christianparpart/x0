# x0 - HTTP Web Server and Framework

[ ![Build status - Travis-ci](https://secure.travis-ci.org/xzero/x0.png) ](http://travis-ci.org/xzero/x0)

- official website: http://xzero.io/ (work in progress)
- github: http://github.com/xzero/x0
- ohloh: http://www.ohloh.net/p/x0
- travis-ci: https://travis-ci.org/xzero/x0

x0 is a low-latency scalable HTTP web server and web service framework
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
- request path aliasing (plugin)
- automatic directory listing generation (plugin)
- apache-style access log (plugin)
- user-directory support (plugin)
- browser match support (plugin)
- customized Expires and Cache-Control response header control (plugin)

# INSTALLATION REQUIREMENTS:

- gcc >= 4.6.0 (for building only)
- libev >= 4.0
- LLVM 3.0 or 3.1 or 3.3 (not 3.2 or 3.4)
- cmake (for building only)
- tbb, Threading Building Blocks (required)
- zlib (optional & recommended, for compression)
- bzip2 (optional & recommended, for compression)
- gnutls (optional & recommended, for SSL/TLS encryption)
- gtest (optional, for unit testing)

# HOW TO BUILD:

    # Install git and clone repository
    apt-get install git
    git clone git://github.com/xzero/x0.git && cd x0
    
    # Installs required dependencies
    sudo apt-get install make cmake gcc g++ libcppunit-dev libgnutls28-dev libgcrypt11-dev \
        libmysqlclient-dev libev-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools0 libtbb-dev libpam-dev
    
    # Installs optional requirements
    sudo apt-get install libmagickwand-dev librrd-dev

    # If you want to built the tests, you must install libgtest-dev and then built it yourself
    sudo apt-get install libgtest-dev
    cd /usr/src/gtest && sudo cmake . && sudo make && sudo cp -vpi libgtest*.a /usr/local/lib/
    
    # Now run cmake to bootstrap build
    cmake -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX=$HOME/local
    
    # Ensure installation target prefix
    mkdir $HOME/local
    
    # Now compiling should just work.
    make && make install
    
    # Run web server on port 8080
    `pwd`/x0d/src/x0d --instant=`pwd`/www/htdocs,8080
    
    # or try its CLI help
    `pwd`/x0d/src/x0d -h

    # have fun hacking.

# How to build with Clang and libc++

You can built x0 with the very latest Clang/LLVM 3.3 branch.
I tested this against the `libstdc++` of GCC 4.7 and not yet `libc++`.

