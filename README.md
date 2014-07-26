# Xzero C++ HTTP Framework

[ ![Build status - Travis-ci](https://secure.travis-ci.org/xzero/libxzero.png) ](http://travis-ci.org/xzero/libxzero)

- official website: http://xzero.io
- github: http://github.com/xzero/libxzero
- travis-ci: https://travis-ci.org/xzero/libxzero

`libxzero` is a thin low-latency and scalable HTTP server framework
written in modern C++.

## Framework Features

- thin and clean core API
- response output filter API
- fully asynchronous response content generation support
- zero-copy networking optimizations
- client-cache aware and partial static file transmission support
- HTTP/1.1, including pipelining

## Hello HTTP World

```cpp
#include <xzero/HttpRequest.h>
#include <xzero/HttpServer.h>
#include <ev++.h>

int main() {
  xzero::HttpServer httpServer(ev::default_loop(0));      // create an HTTP server
  httpServer.setupListener("0.0.0.0", 3000);              // listen on 0.0.0.0:3000

  httpServer.requestHandler = [](xzero::HttpRequest* r) { // setup request handler
    r->status = xzero::HttpStatus::Ok;
    r->write("Hello, HTTP World!\n");
    r->finish();
  };

  return httpServer.run();
}
```

## Installation Requirements

- libbase from https://github.com/xzero/libbase.git
- gcc >= 4.8.0 (for building only, CLANG >= 3.4 is also supported)
- cmake (for building only)
- gtest (optional, for unit testing)

### Building from Source on Ubuntu 14.04:

```sh
# Installs required dependencies
sudo apt-get install make cmake pkg-config git gcc-4.8 g++-4.8 \
    libmysqlclient-dev libev-dev zlib1g-dev libbz2-dev libpcre3-dev

# If you want to built the tests, you must install libgtest-dev
# and then built it yourself
sudo apt-get install libgtest-dev
cd /usr/src/gtest && sudo cmake . && sudo make && \
     sudo cp -vpi libgtest*.a /usr/local/lib/; cd -

# Clone the repository
git clone git://github.com/xzero/libxzero.git && cd libxzero
git clone git://github.com/xzero/libbase.git 3rdparty/libbase

# Now run cmake to bootstrap the build
cmake .

# Now compiling should just work
make && sudo make install

# Try out an example
./examples/hello_http
```

### Build from Sources on Ubuntu 12.04

```sh
# Ensure required GCC (version 4.8)
sudo apt-get install python-software-properties
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install gcc-4.8 g++-4.8

# Installs required dependencies
sudo apt-get install make cmake pkg-config git gcc-4.8 g++-4.8 \
    libmysqlclient-dev libev-dev zlib1g-dev libbz2-dev libpcre3-dev

# If you want to built the tests, you must install libgtest-dev
# and then built it yourself
sudo apt-get install libgtest-dev
cd /usr/src/gtest && sudo cmake -DCMAKE_C_COMPILER=$CC \
      -DCMAKE_CXX_COMPILER=$CXX . && sudo make && \
    sudo cp -vpi libgtest*.a /usr/local/lib/; cd -

# Clone the repository
git clone git://github.com/xzero/libxzero.git && cd libxzero
git clone git://github.com/xzero/libbase.git 3rdparty/libbase

# Now run cmake to bootstrap the build
cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++-4.8 .

# Now compiling should just work
make && sudo make install

# Try out an example
./examples/hello_http
```

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
