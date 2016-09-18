#pragma once

#include <xzero/raft/rpc.h>

namespace xzero {
namespace raft {

class Listener {
 public:
  virtual ~Listener() {}

  virtual void receive(Id from, const VoteRequest& message) = 0;
  virtual void receive(Id from, const VoteResponse& message) = 0;

  virtual void receive(Id from, const AppendEntriesRequest& message) = 0;
  virtual void receive(Id from, const AppendEntriesResponse& message) = 0;

  virtual void receive(Id from, const InstallSnapshotResponse& message) = 0;
  virtual void receive(Id from, const InstallSnapshotRequest& message) = 0;
};

} // namespace raft
} // namespace xzero
