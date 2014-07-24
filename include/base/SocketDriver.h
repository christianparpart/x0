// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_SocketDriver_hpp
#define sw_x0_SocketDriver_hpp (1)

#include <ev++.h>
#include <system_error>
#include <unistd.h>
#include <base/Api.h>

namespace base {

class Socket;
class IPAddress;

class BASE_API SocketDriver {
 public:
  SocketDriver();
  virtual ~SocketDriver();

  virtual bool isSecure() const;
  virtual Socket *create(struct ev_loop *loop, int handle, int af);
  virtual Socket *create(struct ev_loop *loop, IPAddress *ipaddr, int port);
  virtual void destroy(Socket *);
};

}  // namespace base

#endif
