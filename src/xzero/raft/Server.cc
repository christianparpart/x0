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
#include <xzero/MonotonicClock.h>
#include <xzero/StringUtil.h>
#include <xzero/Random.h>
#include <xzero/logging.h>
#include <system_error>

/* TODO:
 * [ ] improve election timeout handling (candidate)
 * [ ] improve heartbeat timeout handling (follower / leader)
 */

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

static Duration cumulativeDuration(Duration base) {
  static Random rng_;
  auto emin = base.milliseconds();
  auto emax = base.milliseconds() * 2;
  auto e = emin + rng_.random64() % (emax - emin);

  return Duration::fromMilliseconds(e);
}

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               Discovery* discovery,
               Transport* transport,
               StateMachine* sm)
      : Server(executor, id, storage, discovery, transport, sm,
               3500_milliseconds,
               3300_milliseconds,
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
      nextHeartbeat_(MonotonicClock::now()),
      timer_(executor, std::bind(&Server::onTimeout, this)),
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
    storage_->initialize(id_, currentTerm());
  } else {
    Id storedId = storage_->loadServerId();
    if (storedId != id_) {
      RAISE_CATEGORY(RaftError::MismatchingServerId, RaftCategory());
    }
  }

  logDebug("raft.Server",
           "Server $0 starts with term $1 and index $2",
           id_, currentTerm(), commitIndex_);

  timer_.setTimeout(heartbeatTimeout_);
  timer_.start();
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

void Server::sendVoteRequest() {
  logDebug("raft.Server", "$0: sendVoteRequest", id_);
  assert(state_ == ServerState::Candidate && "Must be in CANDIDATE state to vote.");

  votesGranted_ = 0;
  setCurrentTerm(currentTerm() + 1);
  votedFor_ = Some(std::make_pair(id_, currentTerm()));

  Option<Index> latestIndex = storage_->latestIndex();

  VoteRequest voteRequest{
    .term         = currentTerm(),
    .candidateId  = id_,
    .lastLogIndex = latestIndex ? *latestIndex : 0,
    .lastLogTerm  = latestIndex ? storage_->getLogEntry(*latestIndex)->term() : 0,
  };

  timer_.setTimeout(cumulativeDuration(electionTimeout_));
  timer_.start();

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      transport_->send(peerId, voteRequest);
    }
  }
}

void Server::onTimeout() {
  switch (state_) {
    case ServerState::Follower:
      logWarning("raft.Server", "$0: Timed out waiting for heartbeat from leader.", id_);
      setState(ServerState::Candidate);
      sendVoteRequest();
      break;
    case ServerState::Candidate:
      logInfo("raft.Server", "$0: Split vote. Reelecting.", id_);
      sendVoteRequest();
      break;
    case ServerState::Leader:
      break;
  }
}

void Server::setState(ServerState newState) {
  logDebug(
      "raft.Server", "Server $0: Switching state from $1 to $2",
      id_,
      state_,
      newState);

  state_ = newState;
}

void Server::setCurrentTerm(Term newTerm) {
  logDebug("raft.Server", "$0: Set term to $1 (from $2)", id_, newTerm, currentTerm_);
  currentTerm_ = newTerm;
}

// {{{ Server: receiver API (invoked by Transport on receiving messages)
void Server::receive(Id from, const VoteRequest& req) {
  logDebug("raft.Server", "$0: received from $1 $2", id_, from, req);

  if (req.term < currentTerm()) {
    logInfo("raft.Server", "$0: declined vote for $1. Smaller voting term.",
            id_, req.candidateId, req.term);
    transport_->send(from, VoteResponse{currentTerm(), false});
    return;
  }

  if (req.term > currentTerm()) {
    logDebug("raft.Server", "$0: found a newer term in remote VoteRequest. Adapting.", id_);
    setCurrentTerm(req.term);
    setState(ServerState::Follower);
    votedFor_ = None();
    timer_.setTimeout(heartbeatTimeout_);
  }

  if (votedFor_.isNone()) {
    logInfo("raft.Server", "$0: accepted vote for $1. I did not vote in this term yet.",
            id_, req.candidateId);
    timer_.touch();
    votedFor_ = Some(std::make_pair(req.candidateId, req.lastLogTerm));
    transport_->send(from, VoteResponse{currentTerm(), true});
    return;
  }

  if (req.candidateId == votedFor_->first && req.lastLogTerm > votedFor_->second) {
    logInfo("raft.Server", "$0: accepted vote for $1. Same candidate & bigger term ($2 > $3).",
            id_, req.candidateId, req.term, currentTerm());
    timer_.touch();
    votedFor_ = Some(std::make_pair(req.candidateId, req.lastLogTerm));
    transport_->send(from, VoteResponse{currentTerm(), true});
    return;
  }

  logInfo("raft.Server", "$0: declined vote for $1.", id_, req.candidateId);
  transport_->send(from, VoteResponse{currentTerm(), false});
}

void Server::receive(Id from, const VoteResponse& resp) {
  assert(state_ == ServerState::Candidate || state_ == ServerState::Leader);

  if (resp.voteGranted) {
    logDebug("raft.Server", "$0: received granted vote from $1", id_, from);
    votesGranted_++;
    if (votesGranted_ >= quorumSize() && state_ == ServerState::Candidate) {
      timer_.cancel();
      setState(ServerState::Leader);
      sendHeartbeat();
    }
  } else {
    logDebug("raft.Server", "$0: in $1 received rejected vote $2", id_, from);
  }
}

void Server::sendHeartbeat() {
  Option<Index> latestIndex = storage_->latestIndex();
  AppendEntriesRequest heartbeat {
    .term         = currentTerm(),
    .leaderId     = id_,
    .prevLogIndex = latestIndex ? *latestIndex : 0,
    .prevLogTerm  = latestIndex ? storage_->getLogEntry(*latestIndex)->term() : 0,
    .entries      = {},
    .leaderCommit = commitIndex_,
  };

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      logDebug("raft.Server", "$0: sendHeartbeat $1", id_, peerId);
      transport_->send(peerId, heartbeat);
    }
  }
}

void Server::receive(Id from, const AppendEntriesRequest& req) {
  logDebug("raft.Server", "$0: received from $1 my term $2/$3, $4 (deadline: $5)",
            id_, from, currentTerm(), state_, req, timer_.isActive());

  timer_.touch();

  if (req.term > currentTerm()) { // new leader detected
    logDebug("raft.Server", "$0: new leader $1 detected with term $2",
        id_, req.leaderId, req.term);
    setCurrentTerm(req.term);

    if (state_ != ServerState::Follower) {
      setState(ServerState::Follower);
    }
  }
}

void Server::receive(Id from, const AppendEntriesResponse& resp) {
  logDebug("raft.Server", "$0: received from $1 $2", id_, from, resp);
  // TODO
}

void Server::receive(Id from, const InstallSnapshotRequest& req) {
  logDebug("raft.Server", "$0: received from $1 $2", id_, from, req);
  // TODO
}

void Server::receive(Id from, const InstallSnapshotResponse& resp) {
  logDebug("raft.Server", "$0: received from $1 $2", id_, from, resp);
  // TODO
}
// }}}

} // namespace raft
} // namespace xzero
