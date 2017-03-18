// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Discovery.h>
#include <xzero/raft/Error.h>
#include <xzero/logging.h>

namespace xzero {
namespace raft {

StaticDiscovery::StaticDiscovery(
    std::initializer_list<std::pair<Id, std::string>>&& list)
  : members_(),
    reverse_() {
  for (auto& m: list) {
    members_[m.first] = m.second;
    reverse_[m.second] = m.first;
  }
}

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

  return std::make_error_code(RaftError::ServerNotFound);
}

Result<Id> StaticDiscovery::getId(const std::string& address) const {
  auto i = reverse_.find(address);
  if (i != reverse_.end()) {
    return Result<Id>(i->second);
  }
  return std::make_error_code(RaftError::ServerNotFound);
}

DnsDiscovery::DnsDiscovery(const std::string& fqdn) {
}

DnsDiscovery::~DnsDiscovery() {
}

std::vector<Id> DnsDiscovery::listMembers() const {
  return {};
}

size_t DnsDiscovery::totalMemberCount() const {
  return 0;
}

Result<std::string> DnsDiscovery::getAddress(Id serverId) const {
  return Result<std::string>("");
}

Result<Id> DnsDiscovery::getId(const std::string& address) const {
  return Result<Id>(0);
}

} // namespace raft
} // namespace xzero
