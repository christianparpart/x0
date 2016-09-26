// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/Server.h>
#include <xzero/raft/rpc.h>
#include <xzero/Duration.h>

namespace xzero {
namespace raft {

class ServerUtil {
 public:
  /**
   * Computes the index that the majority of the given input @p set contains.
   */
  static Index majorityIndexOf(const ServerIndexMap& set);

  static Duration cumulativeDuration(Duration base);
};

} // namespace raft
} // namespace xzero
