// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/testing.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/InetTransport.h>
#include <xzero/raft/StateMachine.h>
#include <xzero/raft/Storage.h>
#include <xzero/raft/Server.h>
#include <xzero/raft/Handler.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/net/LocalConnector.h>

using namespace xzero;
using namespace xzero::raft;

class RaftTestFSM : public StateMachine { // {{{
 public:
  std::error_code loadSnapshot(std::unique_ptr<InputStream>&& input) override;
  std::error_code saveSnapshot(std::unique_ptr<OutputStream>&& output) override;
  raft::Reply applyCommand(const Command& command) override;
};

std::error_code RaftTestFSM::loadSnapshot(std::unique_ptr<InputStream>&& input) {
  return std::error_code();
}

std::error_code RaftTestFSM::saveSnapshot(std::unique_ptr<OutputStream>&& output) {
  return std::error_code();
}

raft::Reply RaftTestFSM::applyCommand(const Command& command) {
  return raft::Reply();
}
// }}}
class RaftTestHandler : public Handler { // {{{
 public:
  RaftTestHandler();

  // Handler override
  HelloResponse handleRequest(const HelloRequest& request) override;
  void handleResponse(Id from, const HelloResponse& response) override;

  VoteResponse handleRequest(Id from,
                     const VoteRequest& request) override;

  void handleResponse(Id from,
                      const VoteResponse& response) override;

  AppendEntriesResponse handleRequest(Id from,
                     const AppendEntriesRequest& request) override;

  void handleResponse(Id from,
                      const AppendEntriesResponse& response) override;

  InstallSnapshotResponse handleRequest(Id from,
                     const InstallSnapshotRequest& request) override;

  void handleResponse(Id from,
                      const InstallSnapshotResponse& response) override;
};

RaftTestHandler::RaftTestHandler() {
}

HelloResponse RaftTestHandler::handleRequest(const HelloRequest& request) {
  return HelloResponse{true, request.psk};
}

void RaftTestHandler::handleResponse(Id from, const HelloResponse& response) {
}

VoteResponse RaftTestHandler::handleRequest(
    Id from,
    const VoteRequest& request) {
  return VoteResponse{0x13, true};
}

void RaftTestHandler::handleResponse(
    Id from,
    const VoteResponse& response) {
}

AppendEntriesResponse RaftTestHandler::handleRequest(
    Id from,
    const AppendEntriesRequest& request) {
  return AppendEntriesResponse{};
}

void RaftTestHandler::handleResponse(
    Id from,
    const AppendEntriesResponse& response) {
}

InstallSnapshotResponse RaftTestHandler::handleRequest(
    Id from,
    const InstallSnapshotRequest& request) {
  return InstallSnapshotResponse{};
}

void RaftTestHandler::handleResponse(
    Id from,
    const InstallSnapshotResponse& response) {
}
// }}}

TEST(raft_InetTransport, handshake) {
  PosixScheduler executor;
  StaticDiscovery discovery { {1, "127.0.0.1:1708"},
                              {2, "127.0.0.2:1708"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));
  auto endpointCreator = [&](const std::string& address) -> RefPtr<EndPoint> {
    return nullptr;
  };
  std::shared_ptr<InetTransport> transport(new InetTransport(
      &discovery, &executor, endpointCreator, connector));
  connector->start();

  RaftTestHandler handler;
  transport->setHandler(&handler);

  RefPtr<LocalEndPoint> cli = connector->createClient("\x06\x07\x42\x03psk");

  executor.runLoop();

  EXPECT_EQ("\x06\x08\x01\x03psk", cli->output());
}

TEST(raft_InetTransport, no_handshake) {
  PosixScheduler executor;
  StaticDiscovery discovery { {1, "127.0.0.1:1708"},
                              {2, "127.0.0.2:1708"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));
  auto endpointCreator = [&](const std::string& address) -> RefPtr<EndPoint> {
    return nullptr;
  };
  std::shared_ptr<InetTransport> transport(new InetTransport(
      &discovery, &executor, endpointCreator, connector));
  connector->start();

  RaftTestHandler handler;
  transport->setHandler(&handler);

  RefPtr<LocalEndPoint> cli = connector->createClient("\x05\x01\x11\x22\x33\x44");

  executor.runLoop();

  EXPECT_TRUE(cli->isClosed());
  EXPECT_EQ("", cli->output());
}

TEST(raft_InetTransport, receive_framed_response) {
  PosixScheduler executor;
  StaticDiscovery discovery { {1, "127.0.0.1:1708"},
                              {2, "127.0.0.2:1708"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));
  auto endpointCreator = [&](const std::string& address) -> RefPtr<EndPoint> {
    return nullptr;
  };
  std::shared_ptr<InetTransport> transport(new InetTransport(
      &discovery, &executor, endpointCreator, connector));
  connector->start();

  RaftTestHandler handler;
  transport->setHandler(&handler);

  RefPtr<LocalEndPoint> cli = connector->createClient(
      "\x06\x07\x42\x03psk"
      "\x05\x01\x11\x22\x33\x44");

  executor.runLoop();

  EXPECT_EQ("\x06\x08\x01\x03psk\x03\x02\x13\x01", cli->output());
}

TEST(raft_InetTransport, unknown_message) {
  PosixScheduler executor;
  StaticDiscovery discovery { {1, "127.0.0.1:1708"},
                              {2, "127.0.0.2:1708"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));
  auto endpointCreator = [&](const std::string& address) -> RefPtr<EndPoint> {
    return nullptr;
  };
  std::shared_ptr<InetTransport> transport(new InetTransport(
      &discovery, &executor, endpointCreator, connector));
  connector->start();

  RaftTestHandler handler;
  transport->setHandler(&handler);

  RefPtr<LocalEndPoint> cli = connector->createClient("\x02\x19\xFF");

  executor.runLoop();

  EXPECT_TRUE(cli->isClosed());
  EXPECT_EQ("", cli->output());
}

TEST(raft_InetTransport, zero_length_message) {
  PosixScheduler executor;
  StaticDiscovery discovery { {1, "127.0.0.1:1708"},
                              {2, "127.0.0.2:1708"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));
  auto endpointCreator = [&](const std::string& address) -> RefPtr<EndPoint> {
    return nullptr;
  };
  std::shared_ptr<InetTransport> transport(new InetTransport(
      &discovery, &executor, endpointCreator, connector));
  connector->start();

  RaftTestHandler handler;
  transport->setHandler(&handler);

  RefPtr<LocalEndPoint> cli = connector->createClient("\x00\x01\x02");

  executor.runLoop();

  EXPECT_TRUE(cli->isClosed());
  EXPECT_EQ("", cli->output());
}
