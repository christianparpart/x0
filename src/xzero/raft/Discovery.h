// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <initializer_list>
#include <vector>

namespace xzero {
namespace raft {

/**
 * API for discovering cluster members.
 */
class Discovery {
 public:
  virtual ~Discovery() {}

  /**
   * Retrieves a list of all candidates in a cluster by their Id.
   */
  virtual std::vector<Id> listMembers() = 0;
};

/**
 * API to static service discovery for the Server.
 */
class StaticDiscovery : public Discovery {
 public:
  StaticDiscovery() : members_() {}
  StaticDiscovery(std::initializer_list<Id>&& list) : members_(list) {}

  void add(Id id);

  std::vector<Id> listMembers() override;

 private:
  std::vector<Id> members_;
};

/**
 * Implements DNS based service discovery that honors SRV records,
 * and if none available, A records.
 */
class DnsDiscovery : public Discovery {
 public:
  DnsDiscovery(const std::string& fqdn);
  ~DnsDiscovery();

  std::vector<Id> listMembers() override;
};

} // namespace raft
} // namespace xzero
