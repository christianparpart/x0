// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/Result.h>
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include <string>

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
  virtual std::vector<Id> listMembers() const = 0;

  /**
   * Retrieves total member count.
   */
  virtual size_t totalMemberCount() const = 0;

  /**
   * Maps a server ID to an address that can be used on the transport layer.
   *
   * The address can be an ip:port pair or a unix domain path or similar.
   */
  virtual Result<std::string> getAddress(Id serverId) const = 0;

  /**
   * Reverse mapping, based on discovery address.
   */
  virtual Result<Id> getId(const std::string& address) const = 0;
};

/**
 * API to static service discovery for the Server.
 */
class StaticDiscovery : public Discovery {
 public:
  StaticDiscovery() : members_() {}
  StaticDiscovery(std::initializer_list<std::pair<Id, std::string>>&& list);

  void add(Id id, const std::string& addr);

  std::vector<Id> listMembers() const override;
  size_t totalMemberCount() const override;
  Result<std::string> getAddress(Id serverId) const override;
  Result<Id> getId(const std::string& address) const override;

 private:
  std::unordered_map<Id, std::string> members_;
  std::unordered_map<std::string, Id> reverse_;
};

/**
 * Implements DNS based service discovery that honors SRV records,
 * and if none available, A records with standard TCP/IP port.
 */
class DnsDiscovery : public Discovery {
 public:
  DnsDiscovery(const std::string& fqdn);
  ~DnsDiscovery();

  std::vector<Id> listMembers() const override;
  size_t totalMemberCount() const override;
  Result<std::string> getAddress(Id serverId) const override;
  Result<Id> getId(const std::string& address) const override;
};

} // namespace raft
} // namespace xzero
