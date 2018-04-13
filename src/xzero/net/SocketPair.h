// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/sysconfig.h>
#include <xzero/defines.h>
#include <xzero/net/Socket.h>

namespace xzero {

class [[nodiscard]] SocketPair {
 public:
  enum BlockingMode { Blocking, NonBlocking };
  explicit SocketPair(BlockingMode bm);
  SocketPair();
  ~SocketPair();

  Socket& left() noexcept { return left_; }
  Socket& right() noexcept { return right_; }

  const Socket& left() const noexcept { return left_; }
  const Socket& right() const noexcept { return right_; }

  void closeLeft();
  void closeRight();

 private:
  Socket left_;
  Socket right_;
};

} // namespace xzero

