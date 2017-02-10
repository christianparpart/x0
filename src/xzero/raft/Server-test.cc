// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Server.h>
#include <xzero/raft/StateMachine.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Transport.h>
#include <xzero/raft/Storage.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/testing.h>
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <limits>

using namespace xzero;

class TestKeyValueStore : public raft::StateMachine { // {{{
 public:
  TestKeyValueStore();

  bool saveSnapshot(std::unique_ptr<OutputStream>&& output) override;
  bool loadSnapshot(std::unique_ptr<InputStream>&& input) override;
  void applyCommand(const raft::Command& serializedCmd) override;

  int get(int key) const;

 private:
  std::unordered_map<int, int> tuples_;
};

TestKeyValueStore::TestKeyValueStore()
    : tuples_() {
}

bool TestKeyValueStore::saveSnapshot(std::unique_ptr<OutputStream>&& output) {
  auto o = [&](const uint8_t* data, size_t len) {
    output->write((const char*) data, len);
  };
  BinaryWriter bw(o);
  for (const auto& pair: tuples_) {
    bw.writeVarUInt(pair.first);
    bw.writeVarUInt(pair.second);
  }
  return true;
}

bool TestKeyValueStore::loadSnapshot(std::unique_ptr<InputStream>&& input) {
  tuples_.clear();

  Buffer buffer;
  for (;;) {
    if (input->read(&buffer, 4096) <= 0) {
      break;
    }
  }

  BinaryReader reader((const uint8_t*) buffer.data(), buffer.size());

  while (reader.pending() > 0) {
    int a = reader.parseVarUInt();
    int b = reader.parseVarUInt();
    if (a < 0 || b < 0) {
      break;
    }
    tuples_[a] = b;
  }
  return true;
}

void TestKeyValueStore::applyCommand(const raft::Command& command) {
  BinaryReader reader(command.data(), command.size());
  int a = reader.parseVarUInt();
  int b = reader.parseVarUInt();
  tuples_[a] = b;
}

int TestKeyValueStore::get(int a) const {
  auto i = tuples_.find(a);
  if (i != tuples_.end())
    return i->second;
  else
    return -1;
}
// }}}

class TestServer { // {{{
 public:
  TestServer(raft::Id id,
             const raft::Discovery* discovery,
             Executor* executor);

  int get(int key) const;
  raft::RaftError set(int key, int value);

  raft::LocalTransport* transport() noexcept { return &transport_; }
  raft::Server* server() noexcept { return &raftServer_; }

 private:
  TestKeyValueStore stateMachine_;
  raft::MemoryStore storage_;
  raft::LocalTransport transport_;
  raft::Server raftServer_;
};

TestServer::TestServer(raft::Id id,
                       const raft::Discovery* discovery,
                       Executor* executor)
  : stateMachine_(),
    storage_(),
    transport_(id, executor),
    raftServer_(executor, id, &storage_, discovery, &transport_, &stateMachine_) {
}

int TestServer::get(int key) const {
  return stateMachine_.get(key);
}

raft::RaftError TestServer::set(int key, int value) {
  raft::Command cmd;
  cmd.push_back(key);
  cmd.push_back(value);
  return raftServer_.sendCommand(std::move(cmd));
}
// }}}

template<typename ServerList>
bool isConsensusReached(const ServerList& servers) {
  size_t leaderCount = 0;
  size_t followerCount = 0;

  for (const auto& s: servers) {
    switch (s->server()->state()) {
      case raft::ServerState::Leader:
        leaderCount++;
        break;
      case raft::ServerState::Follower:
        followerCount++;
        break;
      case raft::ServerState::Candidate:
        break;
    }
  }

  return leaderCount + followerCount == servers.size();
}

TEST(raft_Server, leaderElection) {
  PosixScheduler executor;
  const raft::StaticDiscovery sd = {
    { 1, "127.0.0.1:1042" },
    { 2, "127.0.0.1:1042" },
    { 3, "127.0.0.1:1042" },
  };

  std::vector<std::unique_ptr<TestServer>> servers;
  for (raft::Id id: sd.listMembers()) {
    servers.emplace_back(new TestServer(id, &sd, &executor));
  }

  for (auto& s: servers) {
    s->server()->onStateChanged = [&](raft::Server* s, raft::ServerState oldState) {
      logf("onStateChanged[$0]: $1 ~> $2", s->id(), oldState, s->state());
      if (isConsensusReached(servers)) {
        executor.breakLoop();
      }
    };
    s->server()->onLeaderChanged = [&](raft::Id oldLeaderId) {
      logf("onLeaderChanged[$0]: $1 ~> $2", s->server()->id(), oldLeaderId,
          s->server()->currentLeaderId());
    };

    // register (id,peer) tuples of server peers to this server
    for (auto& t: servers) {
      s->transport()->setPeer(t->server()->id(), t->server());
    }
  }

  for (auto& s: servers) {
    std::error_code ec = s->server()->start();
    ASSERT_TRUE(!ec);
  };

  executor.runLoop();

  // now, leader election must have been taken place
  // 1 leader and 2 followers must exist

  EXPECT_TRUE(isConsensusReached(servers));
}

TEST(raft_Server, startWithLeader) {
  PosixScheduler executor;
  const raft::StaticDiscovery sd = {
    { 1, "127.0.0.1:4201" },
    { 2, "127.0.0.1:4202" },
    { 3, "127.0.0.1:4203" },
  };
  const raft::Id initialLeaderId = 3;

  // create servers
  std::vector<std::unique_ptr<TestServer>> servers;
  for (raft::Id id: sd.listMembers()) {
    servers.emplace_back(new TestServer(id, &sd, &executor));
  }

  // register (id,peer) tuples of server peers to this server
  for (auto& s: servers) {
    s->server()->onStateChanged = [&](raft::Server* s, raft::ServerState oldState) {
      logf("onStateChanged[$0]: $1 ~> $2", s->id(), oldState, s->state());
      if (isConsensusReached(servers)) {
        executor.breakLoop();
      }
    };

    for (auto& t: servers) {
      s->transport()->setPeer(t->server()->id(), t->server());
    }
  }

  for (auto& s: servers) {
    // XXX any (1, 2, 3) should work
    std::error_code ec = s->server()->startWithLeader(initialLeaderId);
    ASSERT_TRUE(!ec);
  };

  executor.runLoop();
}

TEST(raft_Server, AppendEntries) {
}

