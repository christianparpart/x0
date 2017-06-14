// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/Transport.h>
#include <unordered_map>

namespace xzero {

class Executor;

namespace raft {

class Discovery;

/**
 * An in-memory peer-to-peer transport layer (for debugging / unit testing purposes only).
 */
class LocalTransport : public Transport {
 public:
  LocalTransport(Id myId, Executor* executor);

  LocalTransport(const LocalTransport&) = delete;
  LocalTransport& operator=(const LocalTransport&) = delete;

  LocalTransport(LocalTransport&&);
  LocalTransport& operator=(LocalTransport&&);

  void setPeer(Id id, Handler* handler);
  Handler* getPeer(Id id);

  void setHandler(Handler* handler) override;
  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;

 private:
  Id myId_;
  Handler* myHandler_;
  Executor* executor_;
  std::unordered_map<Id, Handler*> peers_;
};

} // namespace raft
} // namespace xzero
