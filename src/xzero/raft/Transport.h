// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>

namespace xzero {
namespace raft {

class Handler;

/**
 * Abstracts peer-to-peer communication between Server instances.
 */
class Transport {
 public:
  virtual ~Transport();

  static const std::string& protocolName();

  virtual void setHandler(Handler* handler) = 0;

  virtual void send(Id target, const VoteRequest& message) = 0;
  virtual void send(Id target, const AppendEntriesRequest& message) = 0;
  virtual void send(Id target, const InstallSnapshotRequest& message) = 0;
};

} // namespace raft
} // namespace xzero
