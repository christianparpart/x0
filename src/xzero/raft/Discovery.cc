// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Discovery.h>
#include <xzero/logging.h>

namespace xzero {
namespace raft {

void StaticDiscovery::add(Id id, const std::string& addr) {
  members_[id] = addr;
}

std::vector<Id> StaticDiscovery::listMembers() const {
  std::vector<Id> result;

  for (const auto& i: members_)
    result.push_back(i.first);

  return result;
}

size_t StaticDiscovery::totalMemberCount() const {
  return members_.size();
}

Result<std::string> StaticDiscovery::getAddress(Id serverId) const {
  auto i = members_.find(serverId);
  if (i != members_.end()) {
    return Result<std::string>(i->second);
  }

  return Failuref("No server found with Id $0.", serverId);
}

} // namespace raft
} // namespace xzero
