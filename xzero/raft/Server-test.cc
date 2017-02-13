// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging.h>
#include <xzero/ExceptionHandler.h>
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

  std::function<void(int, int)> onApplyCommand;

 private:
  std::unordered_map<int, int> tuples_;
};

TestKeyValueStore::TestKeyValueStore()
    : onApplyCommand(),
      tuples_() {
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
  logDebug("TestKeyValueStore", "applyCommand: $0", StringUtil::hexPrint(command.data(), command.size()));
  BinaryReader reader(command.data(), command.size());
  int a = reader.parseVarUInt();
  int b = reader.parseVarUInt();
  logDebug("TestKeyValueStore", "-> applyCommand: $0 = $1", a, b);
  tuples_[a] = b;

  if (onApplyCommand) {
    onApplyCommand(a, b);
  }
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
  std::error_code set(int key, int value);

  raft::LocalTransport* transport() noexcept { return &transport_; }
  raft::Server* server() noexcept { return &raftServer_; }
  TestKeyValueStore* fsm() noexcept { return &stateMachine_; }

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

std::error_code TestServer::set(int key, int value) {
  raft::Command cmd;

  auto o = [&](const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      cmd.push_back(data[i]);
    }
  };

  BinaryWriter bw(o);
  bw.writeVarUInt(key);
  bw.writeVarUInt(value);

  logDebug("TestServer", "set($0, $1): $2", key, value,
      StringUtil::hexPrint(cmd.data(), cmd.size()));

  return raftServer_.sendCommand(std::move(cmd));
}
// }}}
struct TestServerPod { // {{{
  PosixScheduler executor;
  const raft::StaticDiscovery discovery;
  std::vector<std::unique_ptr<TestServer>> servers;

  TestServerPod();

  void enableBreakOnConsensus();
  bool isConsensusReached() const;

  void startWithLeader(raft::Id id);
  void start();

  TestServer* getInstance(raft::Id id);
};

TestServerPod::TestServerPod()
  : executor(std::make_unique<CatchAndLogExceptionHandler>("TestServerPod")),
    discovery{ { 1, "127.0.0.1:4201" },
               { 2, "127.0.0.1:4202" },
               { 3, "127.0.0.1:4203" } },
    servers() {
  // create servers
  for (raft::Id id: discovery.listMembers()) {
    servers.emplace_back(new TestServer(id, &discovery, &executor));
  }

  // register (id,peer) tuples of server peers to this server
  for (auto& s: servers) {
    for (auto& t: servers) {
      s->transport()->setPeer(t->server()->id(), t->server());
    }

    s->server()->onLeaderChanged = [&](raft::Id oldLeaderId) {
      logDebug("TestServerPod", "onLeaderChanged[$0]: $1 ~> $2",
          s->server()->id(), oldLeaderId,
          s->server()->currentLeaderId());
    };
  }
}

void TestServerPod::enableBreakOnConsensus() {
  for (auto& s: servers) {
    s->server()->onStateChanged = [&](raft::Server* s, raft::ServerState oldState) {
      logDebug("TestServerPod", "onStateChanged[$0]: $1 ~> $2", s->id(), oldState, s->state());
      if (isConsensusReached()) {
        executor.breakLoop();
      }
    };
  }
}

bool TestServerPod::isConsensusReached() const {
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

void TestServerPod::startWithLeader(raft::Id initialLeaderId) {
  for (auto& s: servers) {
    std::error_code ec = s->server()->startWithLeader(initialLeaderId);
    ASSERT_TRUE(!ec);
  };
}

void TestServerPod::start() {
  for (auto& s: servers) {
    std::error_code ec = s->server()->start();
    ASSERT_TRUE(!ec);
  };
}

TestServer* TestServerPod::getInstance(raft::Id id) {
  for (auto& i: servers) {
    if (i->server()->id() == id) {
      return i.get();
    }
  }

  return nullptr;
}
// }}}

TEST(raft_Server, leaderElection) {
  TestServerPod pod;
  pod.enableBreakOnConsensus();
  pod.start();
  pod.executor.runLoop();

  // now, leader election must have been taken place
  // 1 leader and 2 followers must exist

  EXPECT_TRUE(pod.isConsensusReached());
}

TEST(raft_Server, startWithLeader) {
  TestServerPod pod;
  pod.enableBreakOnConsensus();
  pod.startWithLeader(1);
  pod.executor.runLoop();
  ASSERT_TRUE(pod.isConsensusReached());
  ASSERT_TRUE(pod.getInstance(1)->server()->isLeader());
}

TEST(raft_Server, AppendEntries) {
  TestServerPod pod;
  pod.enableBreakOnConsensus();
  pod.startWithLeader(1);
  pod.executor.runLoop();
  ASSERT_TRUE(pod.isConsensusReached());
  ASSERT_TRUE(pod.getInstance(1)->server()->isLeader());

  std::error_code err = pod.getInstance(1)->set(2, 4);
  ASSERT_FALSE(err);

  err = pod.getInstance(1)->set(3, 6);
  ASSERT_FALSE(err);

  err = pod.getInstance(1)->set(4, 7);
  ASSERT_FALSE(err);

  int applyCount = 0;
  for (raft::Id id = 1; id <= 3; ++id) {
    pod.getInstance(id)->fsm()->onApplyCommand = [&](int key, int value) {
      logf("onApplyCommand for instance $0 = $1", key, value);
      applyCount++;
      if (applyCount == 9) {
        pod.executor.breakLoop();
      }
    };
  }

  pod.executor.runLoop();

  ASSERT_EQ(4, pod.getInstance(1)->get(2));
  ASSERT_EQ(4, pod.getInstance(2)->get(2));
  ASSERT_EQ(4, pod.getInstance(3)->get(2));

  ASSERT_EQ(6, pod.getInstance(1)->get(3));
  ASSERT_EQ(6, pod.getInstance(2)->get(3));
  ASSERT_EQ(6, pod.getInstance(3)->get(3));

  ASSERT_EQ(7, pod.getInstance(1)->get(4));
  ASSERT_EQ(7, pod.getInstance(2)->get(4));
  ASSERT_EQ(7, pod.getInstance(3)->get(4));
}
