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
               5,                   // maxCommandsPerMessage
               1024,                // maxCommandsSizePerMessage
               250_milliseconds,    // heartbeatTimeout
               100_milliseconds,    // electionTimeout
               500_milliseconds) {  // commitTimeout
}

Server::Server(Executor* executor,
               Id id,
               Storage* storage,
               const Discovery* discovery,
               Transport* transport,
               StateMachine* sm,
               size_t maxCommandsPerMessage,
               size_t maxCommandsSizePerMessage,
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
      maxCommandsPerMessage_(maxCommandsPerMessage),
      maxCommandsSizePerMessage_(maxCommandsSizePerMessage),
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

std::error_code Server::sendCommand(Command&& command) {
  if (state_ != ServerState::Leader)
    return std::make_error_code(RaftError::NotLeading);

  LogEntry entry(currentTerm(), std::move(command));

  // store commands locally first
  std::error_code ec = storage_->appendLogEntry(entry);
  if (ec)
    return ec;

  // replicate logs to each follower
  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      wakeupReplicationTo(peerId);
    }
  }

  // TODO: allow returning log index as result
  // TODO: allow synchronous blocking call

  return std::error_code();
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
      assert(!"should not be needed, as it ought to be maintained per follower");
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
      for (auto& peer: followerTimeouts_)
        peer.second->cancel();
      break;
    case ServerState::Candidate:
      timer_.setTimeout(electionTimeout_);
      for (auto& peer: followerTimeouts_)
        peer.second->cancel();
      break;
    case ServerState::Leader:
      timer_.setTimeout(heartbeatTimeout_);
      timer_.cancel(); // stop global timer, as we use per-peer timers
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

void Server::handleResponse(Id peerId, const HelloResponse& res) {
  // logDebug("raft.Server", "handleResponse: VoteResponse<$0, \"$1\">",
  //     res.success ? "success" : "failed", res.message);
  if (state_ != ServerState::Leader)
    return;

  followerTimeouts_[peerId]->rewind();
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
  if (state_ == ServerState::Leader)
    return;

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
  if (!req.entries.empty()) {
    for (const LogEntry& entry: req.entries) {
      if (index > latestIndex()) {
        // all items are identical between 0 and i are identical
        break;
      }
      if (entry.term() != getLogTerm(index)) {
        logInfo("raft.Server",
                "Truncating at index $0. Local term $1 != leader term $2.",
                getLogTerm(index),
                entry.term(),
                req.term);
        storage_->truncateLog(index - 1);
        break;
      } else {
        logDebug("raft.Server", "found identical logEntry at [$0] $1: $2",
            index, i, entry);
      }
      i++;
      index++;
    }
  }

  // 4. Append any new entries not already in the log
  while (index <= lastIndex) {
    logDebug("raft.Server", "persist logEntry[$0] at index $1/$2, $3", i, index, lastIndex, latestIndex());
    storage_->appendLogEntry(req.entries[i]);
    index++;
    i++;
  }

  assert(lastIndex == latestIndex());

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

bool Server::tryLoadLogEntries(Index first,
                               std::vector<LogEntry>* entries) {
  logDebug("raft.Server", "$0.tryLoadLogEntries: first=$1, latest=$2",
           id_, first, latestIndex());
  size_t totalSize = 0;
  size_t count = std::min((Index) maxCommandsPerMessage_,
                          latestIndex() - first);

  for (size_t i = first; i <= first + count; ++i) {
    logDebug("raft.Server", "$0 try loading log entry $1", id_, i);
    Result<LogEntry> entry = storage_->getLogEntry(i);
    if (entry.isFailure()) {
      logError("raft.Server",
               "Could not retrieve log at index $0. $1",
               i,
               entry.failureMessage());
      return false;
    }

    totalSize += entry->command().size();
    entries->emplace_back(*entry);
  }

  return true;
}

void Server::handleResponse(Id peerId, const AppendEntriesResponse& resp) {
  if (state_ != ServerState::Leader) {
    logWarning("raft.Server", "Received an AppendEntriesResponse from a follower, but I am no leader (anymore).");
    return;
  }

  followerTimeouts_[peerId]->rewind();

  if (resp.success) {
    // If successful: update nextIndex and matchIndex for follower (§5.3)
    // XXX we currently replicate only 1 log entry at a time

    matchIndex_[peerId] = resp.lastLogIndex;
    nextIndex_[peerId] = resp.lastLogIndex + 1;
  } else {
    // If AppendEntries fails because of log inconsistency:
    //  decrement (adjust) nextIndex and retry (§5.3)
    nextIndex_[peerId] = resp.lastLogIndex + 1;
    if (matchIndex_[peerId] != 0) {
      logWarning("raft.Server",
                 "$0: matchIndex[$1] should be 0 (actual $2). Fixing.",
                 id_, peerId, matchIndex_[peerId]);
      matchIndex_[peerId] = 0;
    }
  }

  // update commitIndex to the latest matchIndex the majority of servers provides
  Index newCommitIndex = ServerUtil::majorityIndexOf(matchIndex_);
  if (commitIndex_ != newCommitIndex) {
    logDebug("raft.Server", "$0 ($1) updating commitIndex = $2 (was $3)",
        id_, state_, newCommitIndex, commitIndex_);
    commitIndex_ = newCommitIndex;
    applyLogs();
  }

  // trigger replication feed for peerId
//  heartbeat_->wakeup();
  replicateLogsTo(peerId); // replicate any pending messages to peer, if any
}

InstallSnapshotResponse Server::handleRequest(
    Id peerId,
    const InstallSnapshotRequest& req) {
  timer_.rewind();

  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, req);
  RAISE(NotImplementedError); // TODO
}

void Server::handleResponse(Id peerId, const InstallSnapshotResponse& res) {
  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, res);
  followerTimeouts_[peerId]->rewind();

  RAISE(NotImplementedError); // TODO
}
// }}}
// {{{ Server: receiver helper
void Server::setupLeader() {
  setState(ServerState::Leader);
  currentLeaderId_ = id_;

  const Index theNextIndex = latestIndex() + 1;
  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      nextIndex_[peerId] = theNextIndex;
      matchIndex_[peerId] = 0;
      nextHeartbeats_[peerId] = MonotonicTime::Zero; // force heartbeat
      followerTimeouts_[peerId].reset(new DeadlineTimer(
            executor_,
            std::bind(&Server::onFollowerTimeout, this, peerId),
            heartbeatTimeout_));
      followerTimeouts_[peerId]->start();

      replicateLogsWithWakeupTo(peerId);
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

void Server::wakeupReplicationTo(Id peerId) {
  logDebug("raft.Server", "$0 wakeup replication to $1", id_, peerId);
  wakeups_[peerId].wakeup();
}

void Server::replicateLogsWithWakeupTo(Id peerId) {
  logDebug("raft.Server", "[Wakeup] replicate logs with wakeup to $0!", peerId);

  replicateLogsTo(peerId);

  executor_->executeOnNextWakeup(
      std::bind(&Server::replicateLogsWithWakeupTo, this, peerId),
      &wakeups_[peerId]);
}

void Server::onFollowerTimeout(Id peerId) {
  logDebug("raft.Server", "$0: Maintaining leadership.", id_);
  replicateLogsTo(peerId);
}

void Server::replicateLogsTo(Id peerId) {
  logDebug("raft.Server", "$0 replicating logs to $1", id_, peerId);

  // If last log index ≥ nextIndex for a follower: send
  // AppendEntries RPC with log entries starting at nextIndex
  const Index nextIndex = nextIndex_[peerId];

  // peer is up-to-date and doesn't need a heartbeat either
  if (nextIndex > latestIndex() && nextHeartbeats_[peerId] > MonotonicClock::now()) {
    logDebug("raft.Server", "peer $0 is up-to-date and is within heartbeat time frame", peerId);
    return;
  }

  std::vector<LogEntry> entries;
  if (nextIndex <= latestIndex()) {
    if (!tryLoadLogEntries(nextIndex, &entries)) {
      logDebug("raft.Server", "Failed to load log entries starting at index $0", nextIndex);
      return;
    }
    if (nextIndex + entries.size() < latestIndex()) {
      logWarning("raft.Server",
                 "Too many messages pending for peer $0.",
                 peerId);
    }
  }

  size_t numEntries = entries.size();
  if (numEntries == 0) {
    logDebug("raft.Server", "$0 maintaining leadership to peer $1",
             id_, peerId);
  } else {
    logDebug("raft.Server",
             "replicateLogsTo peer $0: $1 log entries",
             peerId,
             entries.size());
  }

  transport_->send(peerId, AppendEntriesRequest{
      .term = currentTerm(),
      .leaderId = id_,
      .prevLogIndex = nextIndex - 1,
      .prevLogTerm = getLogTerm(nextIndex - 1),
      .entries = entries,
      .leaderCommit = commitIndex_,
  });

  nextIndex_[peerId] += numEntries;
  nextHeartbeats_[peerId] = MonotonicClock::now() + heartbeatTimeout_;
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
             lastApplied_ + 1, *logEntry);
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
