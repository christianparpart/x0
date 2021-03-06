# Xzero HTTP Application Web Server

`x0d` is a thin low-latency and scalable HTTP web server built on-top
of the Xzero C++ HTTP Framework.

It supports a very expressive and natural configuration via
the [Flow configuration language](https://github.com/christianparpart/x0/) and
a number of standard plugins to become *your* web application server.

## Features

- Customizable Error Pages
- Automatic Directory Indexing
- on-the-fly executable upgrade with gracefully finishing currently active requests.
- Easily Extensible via the Powerful Plugin System
- SSL connection encryption
  - SSL Server Name Indication (SNI) extension
- Dynamic Content Compression
- Advanced Dynamic Load Balancer
  - Supporting different backend transports (TCP/IP, and UNIX Domain Sockets).
  - Supporting different backend protocols (HTTP and FastCGI)
  - Advanced Health Monitoring
  - JSON API for retrieving state, stats,
    and reconfiguring clusters (including adding/updating/removing backends).
  - [Client Side Routing support](http://xzero.io/#/article/client-side-routing)
  - Sticky Offline Mode
  - X-Sendfile support, full static file transmission acceleration
  - HAproxy compatible CSV output
  - Object Cache
    - fully runtime configurable
    - client-cache aware
    - life JSON stats
    - supports serving stale objects when no backend is available
    - `Vary` response-header Support.
    - multiple update strategies (locked, shadowed)
- Basic Authentication
  - including PAM authentication
- Request Path Aliasing
- Automatic Directory Listing
- Apache-style access logging
- User-directory Support
- Browser Match Support
- Customizable Expires & Cache-Control Response Header Control
- instant mode (configuration-less basic HTTP serving)
- CGI/1.1 Support

## Installation Requirements

- latest stable CLANG (4.0) or a comparible GCC
- automake / autoconf (for building only)
- zlib (optional & recommended, for compression)
- OpenSSL (optional & recommended, for SSL/TLS encryption)

### Building from Source on Ubuntu 17.04:

```sh
# Installs required dependencies
sudo apt-get install make autoconf automake libtool gcc-4.8 g++-4.8 \
    libmysqlclient-dev zlib1g-dev libbz2-dev pkg-config libssl-dev \
    libfcgi-dev libgoogle-perftools-dev libpam-dev git

# Install git and clone repository
git clone git://github.com/christianparpart/x0.git && cd x0

# Now bootstrap the build (in a sandbox directory)
mkdir -p build && cd build && ../autogen.sh
../configure # <-- your custom configure-flags here

# Now compiling should just work.
make && sudo make install

# Run web server on port 8080
./x0d --instant=`pwd`/www/htdocs,8080

# or try its CLI help
./x0d -h

# have fun hacking and don't forget to checkout the just installed man pages ;-)
```

LICENSE
-------

```
Copyright (c) 2009-2018 Christian Parpart <christian@parpart.family>

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
