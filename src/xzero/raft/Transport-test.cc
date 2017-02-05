// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/testing.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Transport.h>
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
  bool loadSnapshot(std::unique_ptr<std::istream>&& input) override;
  bool saveSnapshot(std::unique_ptr<std::ostream>&& output) override;
  void applyCommand(const Command& command) override;
};

bool RaftTestFSM::loadSnapshot(std::unique_ptr<std::istream>&& input) {
  return false;
}

bool RaftTestFSM::saveSnapshot(std::unique_ptr<std::ostream>&& output) {
  return false;
}

void RaftTestFSM::applyCommand(const Command& command) {
}
// }}}
class RaftTestHandler : public Handler { // {{{
 public:
  RaftTestHandler();

  // Handler override
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

VoteResponse RaftTestHandler::handleRequest(
    Id from,
    const VoteRequest& request) {
  return VoteResponse{0, true};
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

TEST(raft_Transport, init) {
  PosixScheduler executor;
  Id myId = 1;
  StaticDiscovery discovery { {1, "127.0.0.1:1708"},
                              {2, "127.0.0.2:1708"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));
  std::shared_ptr<InetTransport> transport(new InetTransport(
      myId, &discovery, &executor, connector));
  connector->addConnectionFactory(transport);
  connector->start();

  RaftTestHandler handler;
  transport->setHandler(&handler);

  RefPtr<LocalEndPoint> cli = connector->createClient("\x05\x01\x11\x22\x33\x44");

  //executor.runLoopOnce();
  executor.runLoop();

  logf("response: $0", cli->output());
}
