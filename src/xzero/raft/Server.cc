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
      currentTerm_(1),
      votedFor_(),
      commitIndex_(0),
      lastApplied_(0),
      nextIndex_(),
      matchIndex_() {
}

Server::~Server() {
}

size_t Server::quorumSize() const {
  return discovery_->totalMemberCount() / 2 + 1;
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

  logDebug("raft.Server",
           "Server $0 starts with term $1 and index $2",
           id_, currentTerm_, commitIndex_);

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
  assert(state_ == ServerState::Follower);
  logDebug("raft.Server", "$0 onFollowerTimeout", id_);
  setState(ServerState::Candidate);

  sendVoteRequest();
}

void Server::sendVoteRequest() {
  logDebug("raft.Server", "$0 sendVoteRequest", id_);
  assert(state_ == ServerState::Candidate);

  votesGranted_ = 0;
  currentTerm_++;
  votedFor_ = Some(std::make_pair(id_, currentTerm_));

  VoteRequest voteRequest{
    .term         = currentTerm_,
    .candidateId  = id_,
    .lastLogIndex = lastApplied_,
    .lastLogTerm  = lastApplied_ ? storage_->getLogEntry(lastApplied_)->term() : 0,
  };

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      logDebug("raft.Server", "send from $0 to $1: $2", id_, peerId, voteRequest);
      transport_->send(peerId, voteRequest);
    }
  }

  electionTimeoutHandler_ = executor_->executeAfter(
      varyingElectionTimeout(),
      std::bind(&Server::onElectionTimeout, this));
  printf("%d: %p %p\n", id_, this, &electionTimeoutHandler_);
}

void Server::onElectionTimeout() {
  logDebug("raft.Server", "$0 onElectionTimeout: $1", id_, state_);
  logDebug("raft.Server", "handle cancelled? $0", electionTimeoutHandler_->isCancelled());
  printf("%d: %p %p\n", id_, this, &electionTimeoutHandler_);
  assert(state_ == ServerState::Candidate);

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
  logDebug("raft.Server", "$0 received from $1 $2", id_, from, message);

  VoteResponse response;
  response.term = currentTerm_;

  if (message.term < currentTerm_) {
    response.voteGranted = false;
  } else if (votedFor_.isNone()) {
    response.voteGranted = true;
  } else if (message.candidateId == votedFor_->first
          && message.lastLogTerm >= votedFor_->second) {
    response.voteGranted = true;
  } else {
    response.voteGranted = false;
  }

  transport_->send(from, response);
}

void Server::receive(Id from, const VoteResponse& message) {
  logDebug("raft.Server", "$0/$1 received from $2 $3", id_, state_, from, message);
  assert(state_ == ServerState::Candidate);

  if (message.voteGranted) {
    logDebug("raft.Server", "$0 received granted vote from $1", id_, from);
    votesGranted_++;
    if (votesGranted_ >= quorumSize()) {
      electionTimeoutHandler_->cancel();
      setState(ServerState::Leader);
      sendHeartbeat();
    }
  }
}

void Server::sendHeartbeat() {
  AppendEntriesRequest heartbeat {
    .term         = currentTerm_,
    .leaderId     = id_,
    .prevLogIndex = lastApplied_,
    .prevLogTerm  = lastApplied_ ? storage_->getLogEntry(lastApplied_)->term() : 0,
    .entries      = {},
    .leaderCommit = commitIndex_,
  };

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      logDebug("raft.Server", "$0 sendHeartbeat $1", id_, peerId);
      transport_->send(peerId, heartbeat);
    }
  }
}

void Server::receive(Id from, const AppendEntriesRequest& message) {
  logDebug("raft.Server", "$0 received from $1 my term $2/$3, $4", id_, from, currentTerm_, state_, message);

  electionTimeoutHandler_->cancel();

  if (message.term > currentTerm_) { // new leader detected
    logDebug("raft.Server", "$0 new leader $1 detected with term $2",
        id_, message.leaderId, message.term);
    currentTerm_ = message.term;

    if (state_ != ServerState::Follower) {
      setState(ServerState::Follower);
    }
  }

  // TODO
}

void Server::receive(Id from, const AppendEntriesResponse& message) {
  logDebug("raft.Server", "$0 received from $1 $2", id_, from, message);
  // TODO
}

void Server::receive(Id from, const InstallSnapshotRequest& message) {
  logDebug("raft.Server", "$0 received from $1 $2", id_, from, message);
  // TODO
}

void Server::receive(Id from, const InstallSnapshotResponse& message) {
  logDebug("raft.Server", "$0 received from $1 $2", id_, from, message);
  // TODO
}
// }}}

} // namespace raft
} // namespace xzero
