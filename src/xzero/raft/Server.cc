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

  timer_.rewind();

  votesGranted_ = 0;
  setCurrentTerm(currentTerm() + 1);
  votedFor_ = Some(std::make_pair(id_, currentTerm()));

  VoteRequest voteRequest{
    .term         = currentTerm(),
    .candidateId  = id_,
    .lastLogIndex = latestIndex(),
    .lastLogTerm  = getLogTerm(latestIndex()),
  };

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

  // If RPC request or response contains term T > currentTerm:
  // set currentTerm = T, convert to follower (§5.1)
  if (req.term > currentTerm()) {
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
      setupLeader();
    }
  }
}

void Server::receive(Id peerId, const AppendEntriesRequest& req) {
  timer_.rewind();

  // 1. Reply false if term < currentTerm (§5.1)
  if (req.term < currentTerm()) {
    transport_->send(peerId, AppendEntriesResponse{currentTerm(), false});
    return;
  }

  // 2. Reply false if log doesn’t contain an entry at prevLogIndex
  //    whose term matches prevLogTerm (§5.3)
  if (getLogTerm(req.prevLogIndex) != req.prevLogTerm) {
    transport_->send(peerId, AppendEntriesResponse{currentTerm(), false});
    return;
  }

  // If RPC request or response contains term T > currentTerm:
  // set currentTerm = T, convert to follower (§5.1)
  if (req.term > currentTerm()) {
    logDebug("raft.Server", "$0: new leader $1 detected with term $2",
        id_, req.leaderId, req.term);

    currentLeaderId_ = req.leaderId;
    setCurrentTerm(req.term);

    if (state_ != ServerState::Follower) {
      setState(ServerState::Follower);
    }
  }

  // 3. If an existing entry conflicts with a new one (same index
  //    but different terms), delete the existing entry and all that
  //    follow it (§5.3)
  for (const std::shared_ptr<LogEntry>& entry: req.entries) {
    if (entry->term() != getLogTerm(entry->index())) {
      storage_->truncateLog(entry->index());
      break;
    }
  }

  // 4. Append any new entries not already in the log
  for (const std::shared_ptr<LogEntry>& entry: req.entries) {
    storage_->appendLogEntry(*entry);
  }

  // 5. If leaderCommit > commitIndex, set commitIndex =
  //    min(leaderCommit, index of last new entry)
  if (req.leaderCommit > commitIndex_) {
    commitIndex_ = std::min(req.leaderCommit, req.entries.back()->index());
  }

  applyLogs();

  transport_->send(peerId, AppendEntriesResponse{currentTerm(), true});
}

void Server::receive(Id peerId, const AppendEntriesResponse& resp) {
  // logDebug("raft.Server", "$0: ($1) received from $2: $3", id_, state_, peerId, resp);
  assert(state_ == ServerState::Leader);

  timer_.rewind();

  if (resp.success) {
    // If successful: update nextIndex and matchIndex for follower (§5.3)
    // TODO nextIndex_[peerId] = 
    // TODO matchIndex_[peerId] = 
  } else {
    // If AppendEntries fails because of log inconsistency:
    //  decrement nextIndex and retry (§5.3)
    nextIndex_[peerId]--;
    replicateLogsTo(peerId);
  }
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
// {{{ receiver helper
void Server::setupLeader() {
  const Index theNextIndex = latestIndex() + 1;
  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      nextIndex_[peerId] = theNextIndex;
      matchIndex_[peerId] = 0;
    }
  }

  setState(ServerState::Leader);
  currentLeaderId_ = id_;
  sendHeartbeat();
}

void Server::sendHeartbeat() {
  timer_.rewind();

  AppendEntriesRequest heartbeat {
    .term         = currentTerm(),
    .leaderId     = id_,
    .prevLogIndex = latestIndex(),
    .prevLogTerm  = getLogTerm(latestIndex()),
    .entries      = {},
    .leaderCommit = commitIndex_,
  };

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      transport_->send(peerId, heartbeat);
    }
  }
}

Index Server::latestIndex() {
  return storage_->latestIndex();
}

Term Server::getLogTerm(Index index) {
  auto log = storage_->getLogEntry(index);
  if (log) {
    return log->term();
  } else {
    return 0;
  }
}

void Server::replicateLogs() {
  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      replicateLogsTo(peerId);
    }
  }
}

void Server::replicateLogsTo(Id peerId) {
  // If last log index ≥ nextIndex for a follower: send
  // AppendEntries RPC with log entries starting at nextIndex
  Index nextIndex = nextIndex_[peerId];
  if (latestIndex() >= nextIndex) {
    std::vector<std::shared_ptr<LogEntry>> entries;
    for (int i = 0; nextIndex + i <= latestIndex(); ++i) {
      entries.emplace_back(storage_->getLogEntry(nextIndex + i));
    }

    transport_->send(peerId, AppendEntriesRequest{
        .term = currentTerm(),
        .leaderId = id_,
        .prevLogIndex = nextIndex,
        .prevLogTerm = getLogTerm(nextIndex),
        .entries = entries,
        .leaderCommit = commitIndex_,
    });

    matchIndex_[peerId] = latestIndex();
    nextIndex_[peerId] = latestIndex() + 1;
  }
}

void Server::applyLogs() {
  // If commitIndex > lastApplied: increment lastApplied, apply
  // log[lastApplied] to state machine (§5.3)
  while (commitIndex_ > lastApplied_) {
    lastApplied_++;
    std::shared_ptr<LogEntry> logEntry = storage_->getLogEntry(lastApplied_);
    stateMachine_->applyCommand(logEntry->command());
  }
}
// }}}

} // namespace raft
} // namespace xzero
