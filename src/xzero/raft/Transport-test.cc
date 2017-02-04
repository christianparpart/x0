// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/executor/PosixScheduler.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Transport.h>
#include <xzero/raft/Handler.h>
#include <xzero/net/LocalConnector.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::raft;

class RaftTestHandler : public Handler { // {{{
 public:
  RaftTestHandler();

  // Handler override
  void handleRequest(Id from,
                     const VoteRequest& request,
                     VoteResponse* response) override;

  void handleResponse(Id from,
                      const VoteResponse& response) override;

  void handleRequest(Id from,
                     const AppendEntriesRequest& request,
                     AppendEntriesResponse* response) override;

  void handleResponse(Id from,
                      const AppendEntriesResponse& response) override;

  void handleRequest(Id from,
                     const InstallSnapshotRequest& request,
                     InstallSnapshotResponse* response) override;

  void handleResponse(Id from,
                      const InstallSnapshotResponse& response) override;
};

RaftTestHandler::RaftTestHandler() {
}

void RaftTestHandler::handleRequest(
    Id from,
    const VoteRequest& request,
    VoteResponse* response) {
}

void RaftTestHandler::handleResponse(
    Id from,
    const VoteResponse& response) {
}

void RaftTestHandler::handleRequest(
    Id from,
    const AppendEntriesRequest& request,
    AppendEntriesResponse* response) {
}

void RaftTestHandler::handleResponse(
    Id from,
    const AppendEntriesResponse& response) {
}

void RaftTestHandler::handleRequest(
    Id from,
    const InstallSnapshotRequest& request,
    InstallSnapshotResponse* response) {
}

void RaftTestHandler::handleResponse(
    Id from,
    const InstallSnapshotResponse& response) {
}
// }}}

TEST(raft_Transport, init) {
  PosixScheduler executor;
  RaftTestHandler handler;
  Id myId = 1;
  StaticDiscovery discovery { {1, "127.0.0.1:5152"},
                              {2, "127.0.0.2:5152"} };

  std::shared_ptr<LocalConnector> connector(new LocalConnector(&executor));

  std::shared_ptr<InetTransport> transport(new InetTransport(
      myId, &discovery, &handler, &executor, connector));

  connector->addConnectionFactory(transport);

  connector->start();

  RefPtr<LocalEndPoint> cli = connector->createClient("\x05\x01\x11\x22\x33\x44");

  //executor.runLoopOnce();
  executor.runLoop();

  logf("response: $0", cli->output());
}
