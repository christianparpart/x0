// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Discovery.h>

namespace xzero {
namespace raft {

void StaticDiscovery::add(Id id) {
  members_.emplace_back(id);
}

std::vector<Id> StaticDiscovery::listMembers() {
  return members_;
}

} // namespace raft
} // namespace xzero
