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
#include <xzero/raft/LocalTransport.h>
#include <xzero/raft/Storage.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/BufferUtil.h>
#include <xzero/testing.h>
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <limits>
#include <unistd.h>

using namespace xzero;
using raft::RaftError;

class TestKeyValueStore : public raft::StateMachine { // {{{
 public:
  TestKeyValueStore();

  std::error_code saveSnapshot(std::unique_ptr<OutputStream>&& output) override;
  std::error_code loadSnapshot(std::unique_ptr<InputStream>&& input) override;
  raft::Reply applyCommand(const raft::Command& serializedCmd) override;

  int get(int key) const;

  std::function<void(int, int)> onApplyCommand;

 private:
  std::unordered_map<int, int> tuples_;
};

TestKeyValueStore::TestKeyValueStore()
    : onApplyCommand(),
      tuples_() {
}

std::error_code TestKeyValueStore::saveSnapshot(std::unique_ptr<OutputStream>&& output) {
  auto o = [&](const uint8_t* data, size_t len) {
    output->write((const char*) data, len);
  };
  BinaryWriter bw(o);
  for (const auto& pair: tuples_) {
    bw.writeVarUInt(pair.first);
    bw.writeVarUInt(pair.second);
  }
  return std::error_code();
}

std::error_code TestKeyValueStore::loadSnapshot(std::unique_ptr<InputStream>&& input) {
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
  return std::error_code();
}

raft::Reply TestKeyValueStore::applyCommand(const raft::Command& command) {
  logDebug("TestKeyValueStore", "applyCommand: $0", StringUtil::hexPrint(command.data(), command.size()));
  BinaryReader reader(command.data(), command.size());
  int a = reader.parseVarUInt();
  int b = reader.parseVarUInt();
  int oldValue = tuples_[a];

  logDebug("TestKeyValueStore", "-> applyCommand: $0 = $1", a, b);
  tuples_[a] = b;

  if (onApplyCommand) {
    onApplyCommand(a, b);
  }

  raft::Reply reply;
  BinaryWriter(BufferUtil::writer(&reply)).writeVarUInt(oldValue);
  return reply;
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
  Future<raft::Reply> setAsync(int key, int value);

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
    storage_(executor),
    transport_(id, executor),
    raftServer_(id, &storage_, discovery, &transport_, &stateMachine_) {
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

  Result<raft::Reply> result = raftServer_.sendCommand(std::move(cmd));
  return result.error();
}

Future<raft::Reply> TestServer::setAsync(int key, int value) {
  raft::Command cmd;

  auto o = [&](const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      cmd.push_back(data[i]);
    }
  };

  BinaryWriter bw(o);
  bw.writeVarUInt(key);
  bw.writeVarUInt(value);

  logDebug("TestServer", "setAsync($0, $1): $2", key, value,
      StringUtil::hexPrint(cmd.data(), cmd.size()));

  return raftServer_.sendCommandAsync(std::move(cmd));
}
// }}}
struct TestServerPod { // {{{
  ThreadedExecutor executor;
  const raft::StaticDiscovery discovery;
  std::vector<std::unique_ptr<TestServer>> servers;
  Wakeup consensusReached;

  TestServerPod();

  void onApplyCommand(std::function<void(raft::Id, int, int)> fn);
  void enableStopOnConsensus();
  bool isConsensusReached() const;
  raft::Id getLeaderId() const;

  void startWithLeader(raft::Id id);
  void start();
  void stop();
  void waitUntilAllStopped();
  void waitForConsensus();

  TestServer* getInstance(raft::Id id);
  TestServer* getLeader();
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

void TestServerPod::enableStopOnConsensus() {
  for (auto& s: servers) {
    s->server()->onStateChanged = [&](raft::Server* s, raft::ServerState oldState) {
      logDebug("TestServerPod", "onStateChanged[$0]: $1 ~> $2", s->id(), oldState, s->state());
      if (isConsensusReached()) {
        // XXX usleep(Duration(1000_milliseconds).microseconds());
        stop();
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
        logDebug("TestServerPod", "isConsensusReached: leader = $0", s->server()->id());
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

void TestServerPod::onApplyCommand(
    std::function<void(raft::Id, int, int)> callback) {
  for (auto& s: servers) {
    s->fsm()->onApplyCommand = std::bind(callback,
                                         s->server()->id(), 
                                         std::placeholders::_1,
                                         std::placeholders::_2);
  }
}

void TestServerPod::stop() {
  for (auto& s: servers) {
    s->server()->stop();
  }
}

void TestServerPod::waitUntilAllStopped() {
  for (auto& s: servers) {
    s->server()->waitUntilStopped();
  }
  executor.joinAll();
}

void TestServerPod::waitForConsensus() {
  for (auto& s: servers) {
    s->server()->onStateChanged = [&](raft::Server* s, raft::ServerState oldState) {
      logDebug("TestServerPod", "onStateChanged[$0]: $1 ~> $2", s->id(), oldState, s->state());
      if (isConsensusReached()) {
        // XXX usleep(Duration(1000_milliseconds).microseconds());
        consensusReached.wakeup();
      }
    };
  }

  if (!isConsensusReached()) {
    logDebug("TestServerPod", "waitForConsensus...");
    consensusReached.waitForNextWakeup();
    logDebug("TestServerPod", "waitForConsensus... reached!");
  } else {
    logDebug("TestServerPod", "waitForConsensus... reached straight");
  }
}

TestServer* TestServerPod::getLeader() {
  return getInstance(getLeaderId());
}

raft::Id TestServerPod::getLeaderId() const {
  for (const auto& s: servers) {
    if (s->server()->isLeader()) {
      return s->server()->id();
    }
  }
  return 0;
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
  pod.enableStopOnConsensus();
  pod.start();
  pod.waitUntilAllStopped();

  // now, leader election must have been taken place
  // 1 leader and 2 followers must exist

  EXPECT_TRUE(pod.isConsensusReached());
}

TEST(raft_Server, startWithLeader) {
  TestServerPod pod;
  pod.enableStopOnConsensus();
  pod.startWithLeader(1);
  pod.waitUntilAllStopped();
  ASSERT_TRUE(pod.isConsensusReached());
  ASSERT_TRUE(pod.getInstance(1)->server()->isLeader());
}

TEST(raft_Server, AppendEntries_single_entry) {
  TestServerPod pod;
  pod.startWithLeader(1);
  pod.waitForConsensus();

  std::mutex applyCountLock;
  std::atomic<int> applyCount(0);
  pod.onApplyCommand([&](raft::Id serverId, int key, int value) {
    std::lock_guard<std::mutex> _lk(applyCountLock);
    applyCount++;
    logf("onApplyCommand(server: $0, key: $1, value: $2): $3", serverId, key, value, applyCount.load());
    if (applyCount == 3) {
      pod.stop();
    }
  });

  std::error_code err = pod.getLeader()->set(2, 4);
  ASSERT_FALSE(err);

  pod.waitUntilAllStopped();

  ASSERT_EQ(4, pod.getInstance(1)->get(2));
  ASSERT_EQ(4, pod.getInstance(2)->get(2));
  ASSERT_EQ(4, pod.getInstance(3)->get(2));
}

TEST(raft_Server, AppendEntries_batched_entries) {
  TestServerPod pod;
  pod.startWithLeader(1);
  pod.waitForConsensus();

  std::mutex applyCountLock;
  std::atomic<int> applyCount(0);
  pod.onApplyCommand([&](raft::Id serverId, int key, int value) {
    std::lock_guard<std::mutex> _lk(applyCountLock);
    applyCount++;
    logf("onApplyCommand(server: $0, key: $1, value: $2): $3", serverId, key, value, applyCount.load());
    if (applyCount == 9) {
      pod.stop();
    }
  });

  pod.getLeader()->setAsync(1, 11);
  pod.getLeader()->setAsync(2, 22);
  pod.getLeader()->setAsync(3, 33);

  pod.waitUntilAllStopped();

  ASSERT_EQ(11, pod.getInstance(1)->get(1));
  ASSERT_EQ(11, pod.getInstance(2)->get(1));
  ASSERT_EQ(11, pod.getInstance(3)->get(1));

  ASSERT_EQ(22, pod.getInstance(1)->get(2));
  ASSERT_EQ(22, pod.getInstance(2)->get(2));
  ASSERT_EQ(22, pod.getInstance(3)->get(2));

  ASSERT_EQ(33, pod.getInstance(1)->get(3));
  ASSERT_EQ(33, pod.getInstance(2)->get(3));
  ASSERT_EQ(33, pod.getInstance(3)->get(3));
}

TEST(raft_Server, AppendEntries_async_single) {
  TestServerPod pod;
  pod.startWithLeader(1);

  std::mutex applyCountLock;
  std::atomic<int> applyCount(0);
  pod.onApplyCommand([&](raft::Id serverId, int key, int value) {
    std::lock_guard<std::mutex> _lk(applyCountLock);
    applyCount++;
    logf("onApplyCommand(server: $0, key: $1, value: $2): $3", serverId, key, value, applyCount.load());
    if (applyCount == 3) {
      pod.stop();
    }
  });

  Future<raft::Reply> f = pod.getLeader()->setAsync(2, 4);
  f.wait();

  pod.waitUntilAllStopped();

  ASSERT_EQ(4, pod.getInstance(1)->get(2));
  ASSERT_EQ(4, pod.getInstance(2)->get(2));
  ASSERT_EQ(4, pod.getInstance(3)->get(2));
  ASSERT_EQ(0, BinaryReader(f.get()).parseVarUInt());
}

TEST(raft_Server, AppendEntries_async_batched) {
  TestServerPod pod;
  pod.startWithLeader(1);

  std::mutex applyCountLock;
  std::atomic<int> applyCount(0);
  pod.onApplyCommand([&](raft::Id serverId, int key, int value) {
    std::lock_guard<std::mutex> _lk(applyCountLock);
    applyCount++;
    logf("onApplyCommand(server: $0, key: $1, value: $2): $3", serverId, key, value, applyCount.load());
    if (applyCount == 9) {
      pod.stop();
    }
  });


  Future<raft::Reply> f = pod.getInstance(1)->setAsync(1, 11);
  Future<raft::Reply> g = pod.getInstance(1)->setAsync(2, 22);
  Future<raft::Reply> h = pod.getInstance(1)->setAsync(3, 33);

  ASSERT_EQ(0, BinaryReader(f.waitAndGet()).parseVarUInt());
  ASSERT_EQ(0, BinaryReader(g.waitAndGet()).parseVarUInt());
  ASSERT_EQ(0, BinaryReader(h.waitAndGet()).parseVarUInt());

  pod.waitUntilAllStopped();

  ASSERT_EQ(11, pod.getInstance(1)->get(1));
  ASSERT_EQ(11, pod.getInstance(2)->get(1));
  ASSERT_EQ(11, pod.getInstance(3)->get(1));

  ASSERT_EQ(22, pod.getInstance(1)->get(2));
  ASSERT_EQ(22, pod.getInstance(2)->get(2));
  ASSERT_EQ(22, pod.getInstance(3)->get(2));

  ASSERT_EQ(33, pod.getInstance(1)->get(3));
  ASSERT_EQ(33, pod.getInstance(2)->get(3));
  ASSERT_EQ(33, pod.getInstance(3)->get(3));
}

TEST(raft_Server, AppendEntries_update) {
  TestServerPod pod;
  pod.startWithLeader(1);

  int applyCount = 0;
  for (raft::Id id = 1; id <= 3; ++id) {
    pod.getInstance(id)->fsm()->onApplyCommand = [&](int key, int value) {
      logf("onApplyCommand for instance $0 = $1", key, value);
      applyCount++;
      if (applyCount == 6) {
        logf("onApplyCommand: breaking loop with applyCount = $0", applyCount);
        pod.stop();
      }
    };
  }

  Future<raft::Reply> f = pod.getInstance(1)->setAsync(1, 1);
  Future<raft::Reply> g = pod.getInstance(1)->setAsync(1, 2);

  ASSERT_EQ(0, BinaryReader(f.get()).parseVarUInt());
  ASSERT_EQ(1, BinaryReader(g.waitAndGet()).parseVarUInt());
  ASSERT_EQ(2, pod.getLeader()->get(1));
  pod.waitUntilAllStopped();
}

TEST(raft_Server, AppendEntries_not_leading_err) {
  TestServerPod pod;
  pod.startWithLeader(1);
  pod.waitForConsensus();

  std::mutex applyCountLock;
  std::atomic<int> applyCount(0);
  pod.onApplyCommand([&](raft::Id serverId, int key, int value) {
    std::lock_guard<std::mutex> _lk(applyCountLock);
    applyCount++;
    logf("onApplyCommand(server: $0, key: $1, value: $2): $3", serverId, key, value, applyCount.load());
    if (applyCount == 3) {
      pod.stop();
    }
  });

  std::error_code err;
  
  err = pod.getInstance(3)->set(3, 33);
  EXPECT_EQ(std::make_error_code(RaftError::NotLeading), err);

  err = pod.getInstance(2)->set(2, 22);
  EXPECT_EQ(std::make_error_code(RaftError::NotLeading), err);

  err = pod.getInstance(1)->set(1, 11);
  EXPECT_EQ(std::error_code(), err);

  log("waitUntilStopped");
  pod.waitUntilAllStopped();
}
