#pragma once

#include <xzero/raft/rpc.h>

namespace xzero {
namespace raft {

/**
 * API for receiving Raft messages.
 *
 * @see Server
 */
class Listener {
 public:
  virtual ~Listener() {}

  virtual void receive(const HelloRequest& message) = 0;
  virtual void receive(const HelloResponse& message) = 0;

  virtual void receive(const VoteRequest& message) = 0;
  virtual void receive(const VoteResponse& message) = 0;

  virtual void receive(const AppendEntriesRequest& message) = 0;
  virtual void receive(const AppendEntriesResponse& message) = 0;

  virtual void receive(const InstallSnapshotRequest& message) = 0;
  virtual void receive(const InstallSnapshotResponse& message) = 0;
};

} // namespace raft
} // namespace xzero
