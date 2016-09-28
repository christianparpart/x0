// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/net/Connector.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace xzero {

class Executor;
class Connector;
class EndPoint;

namespace raft {

/**
 * Abstracts communication between Server instances.
 */
class Transport {
 public:
  virtual ~Transport();

  // leader
  virtual void send(Id target, const VoteRequest& message) = 0;
  virtual void send(Id target, const AppendEntriesRequest& message) = 0;
  virtual void send(Id target, const InstallSnapshotRequest& message) = 0;

  // follower / candidate
  virtual void send(Id target, const AppendEntriesResponse& message) = 0;
  virtual void send(Id target, const VoteResponse& message) = 0;
  virtual void send(Id target, const InstallSnapshotResponse& message) = 0;
};

class Listener;

/**
 * Implements Raft over TCP/IP.
 */
class InetTransport : public Transport {
 public:
  explicit InetTransport(Id myId, Listener* receiver);
  ~InetTransport();

  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const VoteResponse& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const AppendEntriesResponse& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;
  void send(Id target, const InstallSnapshotResponse& message) override;

 private:
  Id myId_;
  Listener* receiver_;
  std::unique_ptr<Connector> connector_;
  std::unordered_map<Id, EndPoint*> endpoints_;
};

class LocalTransport : public Transport {
 public:
  LocalTransport(Id myId, Executor* executor);

  LocalTransport(const LocalTransport&) = delete;
  LocalTransport& operator=(const LocalTransport&) = delete;

  LocalTransport(LocalTransport&&);
  LocalTransport& operator=(LocalTransport&&);

  void setPeer(Id id, Listener* listener);

  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const VoteResponse& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const AppendEntriesResponse& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;
  void send(Id target, const InstallSnapshotResponse& message) override;

 private:
  Id myId_;
  Executor* executor_;
  std::unordered_map<Id, Listener*> peers_;
};

} // namespace raft
} // namespace xzero
