// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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

struct Follower {
  //! for each server, index of the next log entry
  //! to send to that server (initialized to leader
  //! last log index + 1)
  Index nextIndex;

  //! for each server, index of highest log entry
  //! known to be replicated on server
  //! (initialized to 0, increases monotonically)
  Index matchIndex;
};

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               const Discovery* discovery,
               Transport* transport,
               StateMachine* sm)
      : Server(executor, id, storage, discovery, transport, sm,
               250_milliseconds,
               100_milliseconds,
               500_milliseconds) {
}

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               const Discovery* discovery,
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
      commitIndex_(0),
      lastApplied_(0),
      nextIndex_(),
      matchIndex_() {
  transport_->setHandler(this);
}

Server::~Server() {
}

size_t Server::quorum() const {
  return discovery_->totalMemberCount() / 2 + 1;
}

std::error_code Server::start() {
  std::error_code ec = storage_->initialize(&id_);
  if (ec)
    return ec;

  logDebug("raft.Server",
           "Server $0 starts with term $1 and index $2",
           id(), currentTerm(), commitIndex_);

  timer_.setTimeout(heartbeatTimeout_);
  timer_.start();

  return ec;
}

std::error_code Server::startWithLeader(Id leaderId) {
  std::error_code ec = storage_->initialize(&id_);
  if (ec)
    return ec;

  if (leaderId == id_) {
    logDebug("raft.Server",
             "Server $0 starts with term $1 and index $2 (as leader)",
             id(), currentTerm(), commitIndex_, leaderId);

    setupLeader();
  } else {
    logDebug("raft.Server",
             "Server $0 starts with term $1 and index $2 (as follower)",
             id(), currentTerm(), commitIndex_, leaderId);

    currentLeaderId_ = id_;
    timer_.setTimeout(ServerUtil::cumulativeDuration(heartbeatTimeout_));
    timer_.start();
  }


  return ec;
}

void Server::stop() {
  timer_.cancel();
  // TODO: be more intelligent, maybe telling the others why I go.
}

RaftError Server::sendCommand(Command&& command) {
  if (state_ != ServerState::Leader)
    return RaftError::NotLeading;

  std::vector<std::shared_ptr<LogEntry>> entries;
  entries.emplace_back(new LogEntry(currentTerm(), std::move(command)));

  for (auto& entry: entries)
    storage_->appendLogEntry(*entry);

  // replicate logs to each follower
  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      replicateLogsTo(peerId);
    }
  }

  return RaftError::Success;
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
  storage_->setVotedFor(id_, currentTerm());

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
      logWarning("raft.Server", "$0: Timed out waiting for heartbeat from leader. [$1]", id_, timer_.timeout());
      setState(ServerState::Candidate);
      sendVoteRequest();
      break;
    case ServerState::Candidate: {
      const Duration oldTimeout = timer_.timeout();
      const Duration newTimeout = ServerUtil::alleviatedDuration(electionTimeout_);
      logDebug("raft.Server", "$0: Split vote. Reelecting [$1 ~> $2].", id_, oldTimeout, newTimeout);
      timer_.setTimeout(newTimeout);
      sendVoteRequest();
      break;
    }
    case ServerState::Leader:
      logDebug("raft.Server", "$0: Maintaining leadership.", id_);
      sendHeartbeat();
      break;
  }
}

void Server::setState(ServerState newState) {
  logInfo("raft.Server", "$0: Entering $1 state.", id_, newState);

  ServerState oldState = state_;
  state_ = newState;

  switch (newState) {
    case ServerState::Follower:
      timer_.setTimeout(ServerUtil::cumulativeDuration(heartbeatTimeout_));
      break;
    case ServerState::Candidate:
      timer_.setTimeout(electionTimeout_);
      break;
    case ServerState::Leader:
      timer_.setTimeout(heartbeatTimeout_);
      break;
  }

  if (onStateChanged) {
    executor_->execute(std::bind(onStateChanged, this, oldState));
  }
}

void Server::setCurrentTerm(Term newTerm) {
  storage_->setCurrentTerm(newTerm);
}

// {{{ Server: handler API (invoked by Transport on receiving messages)
HelloResponse Server::handleRequest(const HelloRequest& req) {
  // logDebug("raft.Server", "handleRequest: HelloRequest<$0, \"$1\">",
  //     req.serverId, req.psk);

  return HelloResponse{true, ""};
}

void Server::handleResponse(Id from, const HelloResponse& res) {
  // logDebug("raft.Server", "handleResponse: VoteResponse<$0, \"$1\">", 
  //     res.success ? "success" : "failed", res.message);
}

VoteResponse Server::handleRequest(Id peerId, const VoteRequest& req) {
  timer_.rewind();

  if (req.term < currentTerm()) {
    // decline request as peer's term is older than our currentTerm
    return VoteResponse{currentTerm(), false};
  }

  // If RPC request or response contains term T > currentTerm:
  // set currentTerm = T, convert to follower (§5.1)
  if (req.term > currentTerm()) {
    logDebug("raft.Server", "$0 received term ($1) > currentTerm ($2) from $3. Converting to follower.",
        id_, req.term, currentTerm(), req.candidateId);
    setCurrentTerm(req.term);
    setState(ServerState::Follower);
    storage_->clearVotedFor();

    Id oldLeaderId = currentLeaderId_;
    currentLeaderId_ = req.candidateId;
    if (onLeaderChanged) {
      executor_->execute(std::bind(onLeaderChanged, oldLeaderId));
    }
  }

  const auto votedFor = storage_->votedFor();
  if (votedFor.isNone()) {
    // Accept vote, as we didn't vote in this term yet.
    storage_->setVotedFor(req.candidateId, req.lastLogTerm);
    return VoteResponse{currentTerm(), true};
  }

  if (req.candidateId == votedFor->first && req.lastLogTerm > votedFor->second) {
    // Accept vote. Same vote-candidate and bigger term
    storage_->setVotedFor(req.candidateId, req.lastLogTerm);
    return VoteResponse{currentTerm(), true};
  }

  return VoteResponse{currentTerm(), false};
}

void Server::handleResponse(Id peerId, const VoteResponse& resp) {
  assert(state_ == ServerState::Candidate || state_ == ServerState::Leader);

  if (resp.voteGranted) {
    votesGranted_++;
    if (votesGranted_ >= quorum() && state_ == ServerState::Candidate) {
      setupLeader();
    }
  }
}

AppendEntriesResponse Server::handleRequest(
    Id peerId,
    const AppendEntriesRequest& req) {
  timer_.rewind();

  // 1. Reply false if term < currentTerm (§5.1)
  if (req.term < currentTerm()) {
    return AppendEntriesResponse{currentTerm(), latestIndex(), false};
  }

  // 2. Reply false if log doesn’t contain an entry at prevLogIndex
  //    whose term matches prevLogTerm (§5.3)
  if (getLogTerm(req.prevLogIndex) != req.prevLogTerm) {
    return AppendEntriesResponse{currentTerm(), latestIndex(), false};
  }

  // If RPC request or response contains term T > currentTerm:
  // set currentTerm = T, convert to follower (§5.1)
  if (req.term > currentTerm()) {
    logDebug("raft.Server", "$0: new leader $1 detected with term $2",
        id_, req.leaderId, req.term);

    setCurrentTerm(req.term);

    Id oldLeaderId = currentLeaderId_;
    currentLeaderId_ = req.leaderId;

    if (state_ != ServerState::Follower) {
      setState(ServerState::Follower);
    }

    if (onLeaderChanged) {
      executor_->execute(std::bind(onLeaderChanged, oldLeaderId));
    }
  }

  // 3. If an existing entry conflicts with a new one (same index
  //    but different terms), delete the existing entry and all that
  //    follow it (§5.3)
  const Index lastIndex = req.prevLogIndex + req.entries.size();
  Index index = req.prevLogIndex + 1;
  size_t i = 0;
  if (!req.entries.empty() && getLogTerm(lastIndex) != req.entries.back().term()) {
    for (const LogEntry& entry: req.entries) {
      if (entry.term() != getLogTerm(index)) {
        logDebug("raft.Server",
                 "AppendEntriesRequest: truncating at index %llu, local term %llu, leader term %llu\n",
                 getLogTerm(index), entry.term());
        storage_->truncateLog(index);
        break;
      }
      i++;
      index++;
    }
  }

  // 4. Append any new entries not already in the log
  while (index <= lastIndex) {
    storage_->appendLogEntry(req.entries[i]);
    index++;
    i++;
  }

  // 5. If leaderCommit > commitIndex, set commitIndex =
  //    min(leaderCommit, index of last new entry)
  if (req.leaderCommit > commitIndex_) {
    commitIndex_ = std::min(req.leaderCommit, lastIndex);
    logDebug("raft.Server", "$0 ($1) commitIndex = $2", id_, state_, commitIndex_);
  }

  // Once a follower learns that a log entry is committed,
  // it applies the entry to its local state machine (in log order)
  applyLogs();

  return AppendEntriesResponse{currentTerm(), latestIndex(), true};
}

void Server::handleResponse(Id peerId, const AppendEntriesResponse& resp) {
  logDebug("raft.Server", "$0-to-$1 ($1) received: $3", peerId, id_, state_, resp);
  assert(state_ == ServerState::Leader);

  timer_.rewind();

  if (resp.success) {
    // If successful: update nextIndex and matchIndex for follower (§5.3)
    // XXX we currently replicate only 1 log entry at a time

    matchIndex_[peerId] = resp.latestIndex;
    nextIndex_[peerId] = resp.latestIndex + 1;
  } else {
    // If AppendEntries fails because of log inconsistency:
    //  decrement (adjust) nextIndex and retry (§5.3)
    nextIndex_[peerId] = resp.latestIndex + 1;
    if (matchIndex_[peerId] != 0) {
      logWarning("raft.Server",
                 "$0: matchIndex[$1] should be 0 (actual $2). Fixing.",
                 id_, peerId, matchIndex_[peerId]);
      matchIndex_[peerId] = 0;
    }
  }

  // update commitIndex to the latest matchIndex the majority of servers provides
  commitIndex_ = ServerUtil::majorityIndexOf(matchIndex_);

  logDebug("raft.Server", "$0 ($1) commitIndex = $2 (updated)", 
      id_, state_, commitIndex_); 

  // trigger replication feed for peerId
//  heartbeat_->wakeup();
  replicateLogsTo(peerId); // FIXME
}

InstallSnapshotResponse Server::handleRequest(
    Id peerId,
    const InstallSnapshotRequest& req) {
  timer_.rewind();

  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, req);
  RAISE(NotImplementedError); // TODO
}

void Server::handleResponse(Id peerId, const InstallSnapshotResponse& res) {
  timer_.rewind();

  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, res);
  RAISE(NotImplementedError); // TODO
}
// }}}
// {{{ Server: receiver helper
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
  if (index > storage_->latestIndex())
    return 0;

  return storage_->getLogEntry(index)->term();
}

void Server::replicateLogsTo(Id peerId) {
  // If last log index ≥ nextIndex for a follower: send
  // AppendEntries RPC with log entries starting at nextIndex
  Index nextIndex = nextIndex_[peerId];
  if (nextIndex > latestIndex()) {
    return;
  }

  std::vector<LogEntry> entries;
  for (int i = 0; nextIndex + i <= latestIndex(); ++i) {
    entries.emplace_back(*storage_->getLogEntry(nextIndex + i));
  }

  transport_->send(peerId, AppendEntriesRequest{
      .term = currentTerm(),
      .leaderId = id_,
      .prevLogIndex = nextIndex - 1,
      .prevLogTerm = getLogTerm(nextIndex - 1),
      .entries = entries,
      .leaderCommit = commitIndex_,
  });

  nextIndex_[peerId] = latestIndex() + 1;
}

void Server::applyLogs() {
  logDebug("raft.Server", "Applying logs (commitIndex:$0, lastApplied:$1)",
            commitIndex_, lastApplied_);
  // If commitIndex > lastApplied: increment lastApplied, apply
  // log[lastApplied] to state machine (§5.3)
  while (commitIndex_ > lastApplied_) {
    Result<LogEntry> logEntry = storage_->getLogEntry(lastApplied_ + 1);
    if (logEntry.isFailure()) {
      logError("raft.Server",
               "Failed to apply log index $0. $1",
               lastApplied_ + 1,
               logEntry.failureMessage());
      break;
    }
    logDebug("raft.Server", "applyCommand at index $0: $1",
             lastApplied_, *logEntry);
    lastApplied_++;
    if (logEntry->type() == LOG_COMMAND) {
      stateMachine_->applyCommand(logEntry->command());
    } else {
      // TODO: LOG_PEER_ADD, LOG_PEER_REMOVE
      RAISE(NotImplementedError);
    }
  }
}
// }}}

} // namespace raft
} // namespace xzero
