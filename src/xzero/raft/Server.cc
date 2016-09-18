// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Server.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Error.h>
#include <xzero/raft/StateMachine.h>
#include <xzero/raft/Storage.h>
#include <xzero/raft/Transport.h>
#include <xzero/StringUtil.h>
#include <xzero/MonotonicClock.h>
#include <xzero/logging.h>
#include <system_error>

namespace xzero {

template<>
std::string StringUtil::toString(raft::ServerState s) {
  switch (s) {
    case raft::ServerState::Follower:
      return "Follower";
    case raft::ServerState::Candidate:
      return "Candidate";
    case raft::ServerState::Leader:
      return "Leader";
  }
}

namespace raft {

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               Discovery* discovery,
               Transport* transport,
               StateMachine* sm)
      : Server(executor, id, storage, discovery, transport, sm,
               500_milliseconds,
               300_milliseconds,
               500_milliseconds) {
}

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               Discovery* discovery,
               Transport* transport,
               StateMachine* sm,
               Duration heartbeatTimeout,
               Duration electionTimeout,
               Duration commitTimeout)
    : executor_(executor),
      id_(id),
      storage_(storage),
      discovery_(discovery),
      transport_(transport),
      stateMachine_(sm),
      state_(ServerState::Follower),
      rng_(),
      nextHeartbeat_(MonotonicClock::now()),
      heartbeatTimeout_(heartbeatTimeout),
      electionTimeout_(electionTimeout),
      commitTimeout_(commitTimeout),
      currentTerm_(storage_->loadTerm()),
      votedFor_(),
      commitIndex_(0),
      lastApplied_(0),
      nextIndex_(),
      matchIndex_() {
}

Server::~Server() {
}

void Server::start() {
  if (!storage_->isInitialized()) {
    storage_->initialize(id_, currentTerm_);
  } else {
    Id storedId = storage_->loadServerId();
    if (storedId != id_) {
      RAISE_CATEGORY(RaftError::MismatchingServerId, RaftCategory());
    }
  }

  electionTimeoutHandler_ = executor_->executeAfter(
      electionTimeout_,
      std::bind(&Server::onFollowerTimeout, this));
}

void Server::stop() {
  // TODO
}

void Server::verifyLeader(std::function<void(bool)> callback) {
  if (state_ != ServerState::Leader) {
    callback(false);
  } else if (nextHeartbeat_ < MonotonicClock::now()) {
    callback(true);
  } else {
    verifyLeaderCallbacks_.emplace_back(callback);
  }
}

Duration Server::varyingElectionTimeout() {
  auto emin = electionTimeout_.milliseconds() / 2;
  auto emax = electionTimeout_.milliseconds();
  auto e = emin + rng_.random64() % (emax - emin);

  return Duration::fromMilliseconds(e);
}

void Server::onFollowerTimeout() {
  logDebug("raft.Server", "onFollowerTimeout: $0", id_);
  setState(ServerState::Candidate);

  sendVoteRequest();
}

void Server::sendVoteRequest() {
  currentTerm_++;

  VoteRequest voteRequest;
  voteRequest.term = currentTerm_;
  voteRequest.candidateId = id_;
  voteRequest.lastLogIndex = lastApplied_;
  voteRequest.lastLogTerm = currentTerm_;

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      logDebug("raft.Server", "send from $0 to $1: $2", id_, peerId, voteRequest);
      transport_->send(peerId, voteRequest);
    }
  }

  // if (electionTimeoutHandler_) {
  //   electionTimeoutHandler_->cancel();
  // }
  electionTimeoutHandler_ = executor_->executeAfter(
      varyingElectionTimeout(),
      std::bind(&Server::onElectionTimeout, this));
}

void Server::onElectionTimeout() {
  logDebug("raft.Server", "onElectionTimeout: $0", id_);

  sendVoteRequest();
}

void Server::setState(ServerState newState) {
  logDebug(
      "raft.Server", "Server $0: Switching state from $1 to $2",
      id_,
      state_,
      newState);

  state_ = newState;
}

// {{{ Server: receiver API (invoked by Transport on receiving messages)
void Server::receive(Id from, const VoteRequest& message) {
  logDebug("raft.Server", "receive from $0 $1", from, message);
  electionTimeoutHandler_->cancel();
  // TODO
}

void Server::receive(Id from, const VoteResponse& message) {
  logDebug("raft.Server", "receive from $0 $1", from, message);
  // TODO
}

void Server::receive(Id from, const AppendEntriesRequest& message) {
  logDebug("raft.Server", "receive from $0 $1", from, message);
  // TODO
}

void Server::receive(Id from, const AppendEntriesResponse& message) {
  logDebug("raft.Server", "receive from $0 $1", from, message);
  // TODO
}

void Server::receive(Id from, const InstallSnapshotRequest& message) {
  logDebug("raft.Server", "receive from $0 $1", from, message);
  // TODO
}

void Server::receive(Id from, const InstallSnapshotResponse& message) {
  logDebug("raft.Server", "receive from $0 $1", from, message);
  // TODO
}
// }}}

} // namespace raft
} // namespace xzero
