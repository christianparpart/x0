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
#include <algorithm>
#include <limits>

using namespace xzero;

class TestSystem : public raft::StateMachine { // {{{
 public:
  TestSystem(raft::Id id,
             raft::Discovery* discovery,
             Executor* executor);

  void loadSnapshot(std::unique_ptr<std::istream>&& data) override;
  void applyCommand(const raft::Command& serializedCmd) override;

  int get(int a) {
    if (tuples_.find(a) != tuples_.end())
      return tuples_[a];
    else
      return -1;
  }

  raft::LocalTransport* transport() { return &transport_; }
  raft::Storage* storage() { return &storage_; }
  raft::Server* server() { return &raftServer_; }

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
      transport_(id, executor),
      raftServer_(executor, id, &storage_, discovery, &transport_, this),
      tuples_() {
}

void TestSystem::loadSnapshot(std::unique_ptr<std::istream>&& data) {
  tuples_.clear();
  // TODO:
  // for (size_t i = 0; i < data.size(); i += 2) {
  //   int a = (int) data[i];
  //   int b = (int) data[i + 1];
  //   tuples_[a] = b;
  // }
}

void TestSystem::applyCommand(const raft::Command& command) {
  int a = static_cast<int>(command[0]);
  int b = static_cast<int>(command[1]);
  tuples_[a] = b;
}
// }}}

TEST(raft_Server, leaderElection) {
  PosixScheduler executor;
  raft::StaticDiscovery sd = {
    { 1, "127.0.0.1:1042" },
    { 2, "127.0.0.1:1042" },
    { 3, "127.0.0.1:1042" },
  };

  std::vector<std::unique_ptr<TestSystem>> servers;
  for (raft::Id id: sd.listMembers()) {
    servers.emplace_back(new TestSystem(id, &sd, &executor));
  }

  size_t leaderCount = 0;
  size_t followerCount = 0;

  auto onCandidateUpdate = [&]() {
    logf("onCandidateUpdate: leaders: $0, followers: $1", leaderCount, followerCount);
    if (leaderCount + followerCount == sd.totalMemberCount()) {
      executor.breakLoop(); // quick shutdown
    }
  };

  for (auto& s: servers) {
    // register (id,peer) tuples of server peers to this server
    for (auto& t: servers) {
      s->transport()->setPeer(t->server()->id(), t->server());
    }

    s->server()->onLeader = [&]() { leaderCount++; onCandidateUpdate(); };
    s->server()->onFollower = [&]() { followerCount++; onCandidateUpdate(); };
  }

  for (auto& s: servers) {
    s->server()->start();
  };

  executor.runLoop();

  // now, leader election must have been taken place
  // 1 leader and 2 followers must exist

  logf("leaders: $0, followers: $1", leaderCount, followerCount);
  EXPECT_EQ(1, leaderCount);
  EXPECT_EQ(2, followerCount);
}
