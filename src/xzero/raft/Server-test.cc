// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Server.h>
#include <xzero/raft/StateMachine.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Transport.h>
#include <xzero/raft/Storage.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/testing.h>
#include <initializer_list>
#include <unordered_map>
#include <vector>

using namespace xzero;

class TestSystem : public raft::StateMachine { // {{{
 public:
  TestSystem(raft::Id id,
             raft::Discovery* discovery,
             Executor* executor);

  void loadSnapshotBegin() override;
  void loadSnapshotChunk(const std::vector<uint8_t>& chunk) override;
  void loadSnapshotEnd() override;
  void applyCommand(const raft::Command& serializedCmd) override;

  int get(int a) {
    if (tuples_.find(a) != tuples_.end())
      return tuples_[a];
    else
      return -1;
  }

  raft::LocalTransport& transport() { return transport_; }
  raft::Storage& storage() { return storage_; }
  raft::Server& server() { return raftServer_; }

 private:
  raft::MemoryStore storage_;
  raft::LocalTransport transport_;
  raft::Server raftServer_;
  std::unordered_map<int, int> tuples_;
};

TestSystem::TestSystem(raft::Id id,
                       raft::Discovery* discovery,
                       Executor* executor)
    : storage_(),
      transport_(id),
      raftServer_(executor, id, &storage_, discovery, &transport_, this),
      tuples_() {
}

void TestSystem::loadSnapshotBegin() {
  tuples_.clear();
}

void TestSystem::loadSnapshotChunk(const std::vector<uint8_t>& chunk) {
  // TODO: make me better
  for (size_t i = 0; i < chunk.size(); i += 2) {
    int a = (int) chunk[i];
    int b = (int) chunk[i + 1];
    tuples_[a] = b;
  }
}

void TestSystem::loadSnapshotEnd() {
  // no-op
}

void TestSystem::applyCommand(const raft::Command& command) {
  int a = static_cast<int>(command[0]);
  int b = static_cast<int>(command[1]);
  tuples_[a] = b;
}
// }}}

TEST(raft_Server, testx3) {
  PosixScheduler executor;
  raft::StaticDiscovery sd;

  std::vector<TestSystem> servers = {
    {1, &sd, &executor},
    {2, &sd, &executor},
    {3, &sd, &executor},
  };

  for (TestSystem& s: servers) {
    // register this server to Service Discovery
    sd.add(s.server().id());

    // register (id,peer) tuples of server peers to this server
    for (TestSystem& t: servers) {
      s.transport().setPeer(s.server().id(), &t.server());
    }
  }

  for (TestSystem& s: servers) {
    s.server().start();
  }

  executor.runLoop();
}
