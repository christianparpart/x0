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
    default:
      RAISE_STATUS(InvalidArgumentError);
  }
}

namespace raft {

Server::Server(Id id,
               Storage* storage,
               const Discovery* discovery,
               Transport* transport,
               StateMachine* sm)
      : Server(id, storage, discovery, transport, sm,
               5,                   // maxCommandsPerMessage
               1024,                // maxCommandsSizePerMessage
               500_milliseconds,    // heartbeatTimeout
               250_milliseconds,    // electionTimeout
               500_milliseconds) {  // commitTimeout
}

Server::Server(Id id,
               Storage* storage,
               const Discovery* discovery,
               Transport* transport,
               StateMachine* sm,
               size_t maxCommandsPerMessage,
               size_t maxCommandsSizePerMessage,
               Duration heartbeatTimeout,
               Duration electionTimeout,
               Duration commitTimeout)
    : executor_(std::make_unique<CatchAndLogExceptionHandler>("raft")),
      id_(id),
      currentLeaderId_(0),
      storage_(storage),
      discovery_(discovery),
      transport_(transport),
      stateMachine_(sm),
      state_(ServerState::Follower),
      nextHeartbeat_(MonotonicClock::now()),
      timer_(&executor_, std::bind(&Server::onTimeout, this)),
      verifyLeaderCallbacks_(),
      heartbeatTimeout_(heartbeatTimeout),
      electionTimeout_(electionTimeout),
      commitTimeout_(commitTimeout),
      maxCommandsPerMessage_(maxCommandsPerMessage),
      maxCommandsSizePerMessage_(maxCommandsSizePerMessage),
      running_(false),
      commitIndex_(0),
      lastApplied_(0),
      votesGranted_(0),
      appliedPromises_(),
      onStateChanged(),
      onLeaderChanged() {
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
           id(), currentTerm(), commitIndex());

  executor_.execute(StringUtil::format("apply/$0", id_),
                    std::bind(&Server::applyLogsLoop, this));

  running_ = true;

  timer_.setTimeout(ServerUtil::cumulativeDuration(heartbeatTimeout_));
  timer_.start();

  return ec;
}

std::error_code Server::startWithLeader(Id leaderId) {
  std::error_code ec = storage_->initialize(&id_);
  if (ec)
    return ec;

  executor_.execute(StringUtil::format("apply/$0", id_),
                    std::bind(&Server::applyLogsLoop, this));

  running_ = true;

  if (leaderId == id_) {
    logDebug("raft.Server",
             "Server $0 starts with term $1 and index $2 (as leader)",
             id(), currentTerm(), commitIndex(), leaderId);

    setupLeader();
  } else {
    logDebug("raft.Server",
             "Server $0 starts with term $1 and index $2 (as follower)",
             id(), currentTerm(), commitIndex(), leaderId);

    currentLeaderId_ = id_;

    timer_.setTimeout(ServerUtil::cumulativeDuration(heartbeatTimeout_));
    timer_.start();
  }

  return ec;
}

void Server::stop() {
  logDebug("raft.Server", "$0 Sending STOP event.", id_);
  timer_.cancel();
  running_ = false;
  shutdownWakeup_.wakeup();
}

void Server::waitUntilStopped() {
  if (running_) {
    logDebug("raft.Server", "$0 waitUntilStopped: wait for wakeup", id_);
    shutdownWakeup_.waitForFirstWakeup();
    logDebug("raft.Server", "$0 waitUntilStopped: stop-event received", id_);
  } else {
    logDebug("raft.Server", "$0 waitUntilStopped: already stopped", id_);
  }
}

Result<Reply> Server::sendCommand(Command&& command) {
  return sendCommandAsync(std::move(command)).waitAndGetResult();
}

Future<Reply> Server::sendCommandAsync(Command&& command) {
  Promise<Reply> promise;

  if (state_ != ServerState::Leader) {
    promise.failure(std::make_error_code(RaftError::NotLeading));
    return promise.future();
  }

  LogEntry entry(currentTerm(), std::move(command));

  Future<Index> localAck = storage_->appendLogEntryAsync(entry);

  localAck.onFailure(promise);
  localAck.onSuccess([this, promise](const Index& index) {
    logDebug("raft.Server", "$0 log entry locally applied. Wakup replication threads", id());
    appliedPromises_.insert(std::make_pair(index, promise));

    for (Id peerId: discovery_->listMembers()) {
      if (peerId != id_) {
        wakeupReplicationTo(peerId);
      }
    }
  });

  return promise.future();
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

  timer_.setTimeout(ServerUtil::alleviatedDuration(electionTimeout_));
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
      executor_.execute(StringUtil::format("voter/$0", peerId),
                        [this, peerId, voteRequest]() {
                          transport_->send(peerId, voteRequest);
                        });
    }
  }
}

void Server::onTimeout() {
  std::lock_guard<decltype(serverLock_)> _lk(serverLock_);

  if (!running_)
    return;

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
      logError("raft.Error", "Internal error in Server ID $0", id_);
      assert(!"Receiving timeout even though we're LEADER");
      break;
  }
}

bool Server::setState(ServerState newState) {
  if (newState == state_) {
    // logWarning("raft.Server", "$0: Attempt to enter already entered state $1.",
    //            id_, newState);
    return false;
  }

  ServerState oldState = state_;
  state_ = newState;
  logInfo("raft.Server", "$0: Entering $1 state (was $2).", id_, newState, oldState);

  if (onStateChanged) {
    onStateChanged(this, oldState);
  }

  return true;
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
}

VoteResponse Server::handleRequest(Id peerId, const VoteRequest& req) {
  timer_.touch();
  std::lock_guard<decltype(serverLock_)> _lk(serverLock_);

  if (req.term < currentTerm()) {
    // decline request as peer's term is older than our currentTerm
    return VoteResponse{currentTerm(), false};
  }

  // If RPC request or response contains term T > currentTerm:
  // set currentTerm = T, convert to follower (§5.1)
  if (req.term > currentTerm()) {
    logDebug("raft.Server", "$0 received term ($1) > currentTerm ($2) from $3. Converting to follower.",
        id_, req.term, currentTerm(), req.candidateId);

    convertToFollower(req.candidateId, req.term);
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
  std::lock_guard<decltype(serverLock_)> _lk(serverLock_);

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
  timer_.touch();
  std::lock_guard<decltype(serverLock_)> _lk(serverLock_);

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

    convertToFollower(req.leaderId, req.term);
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
    logDebug("raft.Server", "$0 persist logEntry[$1] at index $2/$3, $4", id(), i, index, lastIndex, latestIndex());
    storage_->appendLogEntry(req.entries[i]);
    index++;
    i++;
  }

  assert(lastIndex == latestIndex());

  // 5. If leaderCommit > commitIndex, set commitIndex =
  //    min(leaderCommit, index of last new entry)
  if (req.leaderCommit > commitIndex_) {
    commitIndex_ = std::min(req.leaderCommit, lastIndex);
    logDebug("raft.Server", "$0 ($1) commitIndex = $2", id(), state(), commitIndex());

    Future<Reply> applied = appliedPromises_[commitIndex_].future();

    // Once a follower learns that a log entry is committed,
    // it applies the entry to its local state machine (in log order)
    applyLogsWakeup_.wakeup();

    // block until also applied (may involves disk i/o)
    applied.wait();
  }

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

    if (totalSize > maxCommandsSizePerMessage_) {
      break;
    }

    entries->emplace_back(*entry);
  }

  return true;
}

void Server::handleResponse(Id peerId, const AppendEntriesResponse& resp) {
  std::lock_guard<decltype(serverLock_)> _lk(serverLock_);

  if (state_ != ServerState::Leader) {
    logWarning("raft.Server", "$0 Received an AppendEntriesResponse from peer $1, but I am no leader (anymore).", id_, peerId);
    return;
  }

  FollowerState& followerState = followers_[peerId];

  if (resp.success) {
    // If successful: update nextIndex and matchIndex for follower (§5.3)
    followerState.nextHeartbeat = MonotonicClock::now() + heartbeatTimeout_;
    followerState.nextIndex = resp.lastLogIndex + 1;
    followerState.matchIndex = resp.lastLogIndex;
  } else {
    // If AppendEntries fails because of log inconsistency:
    //  decrement (adjust) nextIndex and retry (§5.3)
    followerState.nextHeartbeat = MonotonicClock::now();
    followerState.nextIndex = resp.lastLogIndex + 1;
    if (followerState.matchIndex != 0) {
      logWarning("raft.Server",
                 "$0: matchIndex[$1] should be 0 (actual $2). Fixing.",
                 id_, peerId, followerState.matchIndex);
      followerState.matchIndex = 0;
    }
  }

  // update commitIndex to the latest matchIndex the majority of servers provides
  Index newCommitIndex = computeCommitIndex();
  if (commitIndex_ != newCommitIndex) {
    logDebug("raft.Server", "$0 ($1) updating commitIndex = $2 (was $3)",
        id(), state(), newCommitIndex, commitIndex());
    commitIndex_ = newCommitIndex;
    applyLogsWakeup_.wakeup();
  }
}

Index Server::computeCommitIndex() {
  // smallest matchIndex
  const Index low = std::min_element(followers_.begin(),
                                     followers_.end(),
                                     [](const auto& a, const auto& b) -> bool {
                                       return a.second.matchIndex <
                                              b.second.matchIndex;
                                     })->second.matchIndex;

  // biggest matchIndex
  const Index high = std::max_element(followers_.begin(),
                                      followers_.end(),
                                      [](const auto& a, const auto& b) -> bool {
                                        return a.second.matchIndex <
                                               b.second.matchIndex;
                                      })->second.matchIndex;

  const size_t quorum = followers_.size() / 2 + 1;

  // logDebug("raft.Server", "computeCommitIndex: min=$0, max=$1, quorum=$2",
  //          low, high, quorum);

  Index result = low;
  for (Index N = low; N <= high; N++) {
    size_t ok = 0;

    for (const auto& follower: followers_) {
      if (follower.second.matchIndex >= N) {
        ok++;
      }
    }

    if (ok >= quorum && N > result) {
      result = N;
    }
  }

  return result;
}

InstallSnapshotResponse Server::handleRequest(
    Id peerId,
    const InstallSnapshotRequest& req) {
  timer_.touch();
  std::lock_guard<decltype(serverLock_)> _lk(serverLock_);

  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, req);
  RAISE(NotImplementedError); // TODO
}

void Server::handleResponse(Id peerId, const InstallSnapshotResponse& res) {
  logDebug("raft.Server", "$0: received from $1: $2", id_, peerId, res);

  RAISE(NotImplementedError); // TODO
}
// }}}
// {{{ Server: receiver helper
void Server::convertToFollower(Id newLeaderId, Term leaderTerm) {
  setState(ServerState::Follower);

  setCurrentTerm(leaderTerm);
  storage_->clearVotedFor();

  Id oldLeaderId = currentLeaderId_;
  currentLeaderId_ = newLeaderId;

  if (onLeaderChanged) {
    onLeaderChanged(oldLeaderId);
  }

  timer_.setTimeout(ServerUtil::cumulativeDuration(heartbeatTimeout_));
  timer_.start();
}

void Server::setupLeader() {
  logDebug("raft.Server", "$0 setupLeader", id_);
  timer_.cancel();
  currentLeaderId_ = id_;

  setState(ServerState::Leader);

  for (Id peerId: discovery_->listMembers()) {
    if (peerId != id_) {
      logDebug("raft.Server", "$0 starting replication loop for peer $1", id_, peerId);
      executor_.execute(StringUtil::format("replicate/$0", peerId),
                        std::bind(&Server::replicationLoop, this, peerId));
    }
  }
}

void Server::replicationLoop(Id followerId) {
  logInfo("raft.Server", "$0 Starting worker: replication/$1", id_, followerId);
  FollowerState& followerState = followers_[followerId];

  followerState.nextIndex = latestIndex() + 1;
  followerState.matchIndex = 0;

  while (running_) {
    const MonotonicTime now = MonotonicClock::now();
    const Duration timeout =
        followerState.nextHeartbeat > now ? followerState.nextHeartbeat - now
                                          : Duration::Zero;

    if (timeout > Duration::Zero) {
      logDebug("raft.Server", "$0 replicationLoop/$1: waitFor $2", id_, followerId, timeout);
      // waits until a new log entry arrived, or
      // a forced heartbeat comes in.
      followerState.wakeup.waitFor(timeout);
    }

    // replicate all pending logs or submit a heartbeat (empty message)
    replicateLogsTo(followerId);
  }
  logInfo("raft.Server", "$0 Stopping worker: replication/$1", id_, followerId);
}

void Server::wakeupReplicationTo(Id peerId) {
  FollowerState& followerState = followers_[peerId];
  logDebug("raft.Server", "$0 wakeup replication to $1", id_, peerId);
  followerState.wakeup.wakeup();
}

void Server::replicateLogsTo(Id peerId) {
  FollowerState& followerState = followers_[peerId];
  //logDebug("raft.Server", "$0 replicating logs to $1", id_, peerId);

  // If last log index ≥ nextIndex for a follower: send
  // AppendEntries RPC with log entries starting at nextIndex
  const Index nextIndex = followerState.nextIndex;
  const Index prevLogIndex = nextIndex - 1;

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
             "$0 replicating $1 log entries to peer $2",
             id_,
             entries.size(),
             peerId);
  }

  AppendEntriesRequest req{ currentTerm(),
                            id_,
                            prevLogIndex,
                            getLogTerm(prevLogIndex),
                            commitIndex(),
                            entries };
  transport_->send(peerId, req);

  // transport_->send(peerId, AppendEntriesRequest{
  //     .term = currentTerm(),
  //     .leaderId = id_,
  //     .prevLogIndex = prevLogIndex,
  //     .prevLogTerm = getLogTerm(prevLogIndex),
  //     .leaderCommit = commitIndex(),
  //     .entries = entries,
  // });

  followerState.nextIndex += numEntries;
  followerState.nextHeartbeat = MonotonicClock::now() + heartbeatTimeout_;
}

Index Server::latestIndex() {
  return storage_->latestIndex();
}

Term Server::getLogTerm(Index index) {
  if (index <= storage_->latestIndex())
    return storage_->getLogEntry(index)->term();
  else
    return 0;
}

void Server::applyLogsLoop() {
  logInfo("raft.Server", "$0 Starting worker: applyLogs", id_);

  shutdownWakeup_.onWakeup(shutdownWakeup_.generation(), [&]() {
    applyLogsWakeup_.wakeup();
  });

  while (running_) {
    applyLogsWakeup_.waitForNextWakeup();
    applyLogs();
  }

  logInfo("raft.Server", "$0 Stopping worker: applyLogs.", id_);
}

void Server::applyLogs() {
  logDebug("raft.Server", "$0 Applying logs (commitIndex:$1, lastApplied:$2)",
            id(), commitIndex(), lastApplied());
  // If commitIndex > lastApplied: increment lastApplied, apply
  // log[lastApplied] to state machine (§5.3)
  while (commitIndex_ > lastApplied_) {
    const Index index = lastApplied_ + 1;
    Result<LogEntry> logEntry = storage_->getLogEntry(index);
    if (logEntry.isFailure()) {
      logError("raft.Server",
               "Failed to apply log index $0. $1",
               index,
               logEntry.failureMessage());
      break;
    }

    logDebug("raft.Server", "$0 applyCommand at index $1: $2", id(), index, *logEntry);

    if (logEntry->type() == LOG_COMMAND) {
      Reply reply = stateMachine_->applyCommand(logEntry->command());
      auto i = appliedPromises_.find(index);
      if (i != appliedPromises_.end()) {
        logDebug("raft.Server", "$0 applyCommand: fulfilling promise", id());
        Promise<Reply> promise = i->second;
        appliedPromises_.erase(i);
        promise.success(reply);
      } else {
        logDebug("raft.Server", "$0 applyCommand: no promise to fulfill", id());
      }
    } else {
      // TODO: LOG_PEER_ADD, LOG_PEER_REMOVE
      RAISE(NotImplementedError);
    }

    lastApplied_ = index; // equals lastApplied_++
  }
}
// }}}

} // namespace raft
} // namespace xzero
