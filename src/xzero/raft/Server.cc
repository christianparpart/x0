// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Server.h>
#include <xzero/raft/ServerUtil.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Error.h>
#include <xzero/raft/StateMachine.h>
#include <xzero/raft/Storage.h>
#include <xzero/raft/Transport.h>
#include <xzero/MonotonicClock.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <system_error>
#include <algorithm>

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

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               Discovery* discovery,
               Transport* transport,
               StateMachine* sm)
      : Server(executor, id, storage, discovery, transport, sm,
               100_milliseconds,
               1000_milliseconds,
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
      currentLeaderId_(0),
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

size_t Server::quorum() const {
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

  // logDebug("raft.Server",
  //          "Server $0 starts with term $1 and index $2",
  //          id_, currentTerm(), commitIndex_);

  timer_.setTimeout(heartbeatTimeout_);
  timer_.start();
}

void Server::stop() {
  timer_.cancel();
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
  assert(state_ == ServerState::Candidate && "Must be in CANDIDATE state to vote.");

  votesGranted_ = 0;
  setCurrentTerm(currentTerm() + 1);
  votedFor_ = Some(std::make_pair(id_, currentTerm()));

  LogInfo lastLogInfo = storage_->lastLogInfo();
  VoteRequest voteRequest{
    .term         = currentTerm(),
    .candidateId  = id_,
    .lastLogIndex = lastLogInfo.index,
    .lastLogTerm  = lastLogInfo.term,
  };

  timer_.rewind();

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      transport_->send(peerId, voteRequest);
    }
  }
}

void Server::onTimeout() {
  switch (state_) {
    case ServerState::Follower:
      // logWarning("raft.Server", "$0: Timed out waiting for heartbeat from leader.", id_);
      setState(ServerState::Candidate);
      sendVoteRequest();
      break;
    case ServerState::Candidate:
      // logInfo("raft.Server", "$0: Split vote. Reelecting.", id_);
      sendVoteRequest();
      break;
    case ServerState::Leader:
      // logDebug("raft.Server", "$0: Maintaining leadership.", id_);
      sendHeartbeat();
      break;
  }
}

void Server::setState(ServerState newState) {
  logInfo("raft.Server", "$0: Entering $1 state.", id_, newState);

  state_ = newState;

  switch (newState) {
    case ServerState::Follower:
      timer_.setTimeout(ServerUtil::cumulativeDuration(heartbeatTimeout_));
      break;
    case ServerState::Candidate:
      timer_.setTimeout(ServerUtil::cumulativeDuration(electionTimeout_));
      break;
    case ServerState::Leader:
      timer_.setTimeout(heartbeatTimeout_);
      break;
  }
}

void Server::setCurrentTerm(Term newTerm) {
  currentTerm_ = newTerm;
  storage_->saveTerm(newTerm);
}

// {{{ Server: receiver API (invoked by Transport on receiving messages)
void Server::receive(Id peerId, const VoteRequest& req) {
  if (req.term < currentTerm()) {
    transport_->send(peerId, VoteResponse{currentTerm(), false});
    return;
  }

  if (req.term > currentTerm()) {
    // Found a newer term than our current. Adapting.
    setCurrentTerm(req.term);
    setState(ServerState::Follower);
    votedFor_ = None();
  }

  if (votedFor_.isNone()) {
    // Accept vote, as we didn't vote in this term yet.
    timer_.rewind();
    votedFor_ = Some(std::make_pair(req.candidateId, req.lastLogTerm));
    transport_->send(peerId, VoteResponse{currentTerm(), true});
    return;
  }

  if (req.candidateId == votedFor_->first && req.lastLogTerm > votedFor_->second) {
    // Accept vote. Same vote-candidate and bigger term
    timer_.rewind();
    votedFor_ = Some(std::make_pair(req.candidateId, req.lastLogTerm));
    transport_->send(peerId, VoteResponse{currentTerm(), true});
    return;
  }

  transport_->send(peerId, VoteResponse{currentTerm(), false});
}

void Server::receive(Id peerId, const VoteResponse& resp) {
  assert(state_ == ServerState::Candidate || state_ == ServerState::Leader);

  if (resp.voteGranted) {
    votesGranted_++;
    if (votesGranted_ >= quorum() && state_ == ServerState::Candidate) {
      setState(ServerState::Leader);
      currentLeaderId_ = id_;
      sendHeartbeat();
    }
  }
}

void Server::sendHeartbeat() {
  LogInfo lastLogInfo = storage_->lastLogInfo();
  AppendEntriesRequest heartbeat {
    .term         = currentTerm(),
    .leaderId     = id_,
    .prevLogIndex = lastLogInfo.index,
    .prevLogTerm  = lastLogInfo.term,
    .entries      = {},
    .leaderCommit = commitIndex_,
  };

  timer_.rewind();

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      transport_->send(peerId, heartbeat);
    }
  }
}

void Server::receive(Id peerId, const AppendEntriesRequest& req) {
  timer_.rewind();

  if (req.term < currentTerm()) {
    transport_->send(peerId, AppendEntriesResponse{currentTerm(), false});
    return;
  }

  LogInfo localPos = storage_->lastLogInfo();
  LogInfo peerPos = LogInfo{
    .term   = req.prevLogTerm,
    .index  = req.prevLogIndex,
  };

  if (localPos < peerPos) {
    // locally persisted logs are a way too old compared to peer's last log pos
    transport_->send(peerId, AppendEntriesResponse{currentTerm(), false});
    return;
  }

  // delete any entry after peer's position
  storage_->truncateLog(req.prevLogIndex);

  if (req.term > currentTerm()) { // new leader detected
    logDebug("raft.Server", "$0: new leader $1 detected with term $2",
        id_, req.leaderId, req.term);

    currentLeaderId_ = req.leaderId;
    setCurrentTerm(req.term);

    if (state_ != ServerState::Follower) {
      setState(ServerState::Follower);
    }
  }

  // Term term;                     // leader's term
  // Id leaderId;                   // so follower can redirect clients
  // Index prevLogIndex;            // index of log entry immediately proceding new ones
  // Term prevLogTerm;              // term of prevLogIndex entry
  // std::vector<LogEntry> entries; // log entries to store (empty for heartbeat; may send more than one for efficiency)
  // Index leaderCommit;            // leader's commitIndex

  for (const LogEntry& entry: req.entries) {
    storage_->appendLogEntry(entry);
  }

  // while (commitIndex() < req.leaderCommit)
  //   ;

  // If commitIndex > lastApplied: increment lastApplied, apply
  // log[lastApplied] to state machine (§5.3)
  while (commitIndex_ > lastApplied_) {
    lastApplied_++;
    std::shared_ptr<LogEntry> logEntry = storage_->getLogEntry(lastApplied_);
    stateMachine_->applyCommand(logEntry->command());
  }

  transport_->send(peerId, AppendEntriesResponse{currentTerm(), true});
}

void Server::receive(Id peerId, const AppendEntriesResponse& resp) {
  timer_.rewind();

  //logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, resp);
  // TODO
}

void Server::receive(Id peerId, const InstallSnapshotRequest& req) {
  timer_.rewind();

  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, req);
  // TODO
}

void Server::receive(Id peerId, const InstallSnapshotResponse& resp) {
  timer_.rewind();

  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, resp);
  // TODO
}
// }}}

} // namespace raft
} // namespace xzero
