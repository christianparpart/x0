// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RaftServer.h>
#include <xzero/StringUtil.h>
#include <xzero/MonotonicClock.h>
#include <system_error>

namespace xzero {

RaftServer::RaftServer(Executor* executor,
                       Id id,
                       Storage* storage,
                       Discovery* discovery,
                       Transport* transport,
                       StateMachine* sm)
      : RaftServer(executor, id, storage, discovery, transport, sm,
                   500_milliseconds,
                   300_milliseconds,
                   500_milliseconds) {
}

RaftServer::RaftServer(Executor* executor,
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
      state_(FOLLOWER),
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

RaftServer::~RaftServer() {
}

void RaftServer::start() {
  if (!storage_->isInitialized()) {
    storage_->initialize(id_, currentTerm_);
  } else {
    Id storedId = storage_->loadServerId();
    if (storedId != id_) {
      RAISE_CATEGORY(RaftError::MismatchingServerId, RaftCategory());
    }
  }
  // TODO: add timeout triggers: varyingElectionTimeout
}

void RaftServer::stop() {
  // TODO
}

void RaftServer::verifyLeader(std::function<void(bool)> callback) {
  if (state_ != LEADER) {
    callback(false);
  } else if (nextHeartbeat_ < MonotonicClock::now()) {
    callback(true);
  } else {
    verifyLeaderCallbacks_.emplace_back(callback);
  }
}

Duration RaftServer::varyingElectionTimeout() {
  auto emin = electionTimeout_.milliseconds() / 2;
  auto emax = electionTimeout_.milliseconds();
  auto e = emin + rng_.random64() % (emax - emin);

  return Duration::fromMilliseconds(e);
}

// {{{ RaftServer: receiver API (invoked by Transport on receiving messages)
void RaftServer::receive(Id from, const VoteRequest& message) {
  // TODO
}

void RaftServer::receive(Id from, const VoteResponse& message) {
  // TODO
}

void RaftServer::receive(Id from, const AppendEntriesRequest& message) {
  // TODO
}

void RaftServer::receive(Id from, const AppendEntriesResponse& message) {
  // TODO
}

void RaftServer::receive(Id from, const InstallSnapshotRequest& message) {
  // TODO
}

void RaftServer::receive(Id from, const InstallSnapshotResponse& message) {
  // TODO
}
// }}}
// {{{ RaftServer::StaticDiscovery
void RaftServer::StaticDiscovery::add(Id id) {
  members_.emplace_back(id);
}

std::vector<RaftServer::Id> RaftServer::StaticDiscovery::listMembers() {
  return members_;
}
// }}}
// {{{ LocalTransport
RaftServer::LocalTransport::LocalTransport(Id localId)
    : localId_(localId) {
}

void RaftServer::LocalTransport::send(Id target, const VoteRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const VoteResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const AppendEntriesRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const AppendEntriesResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const InstallSnapshotRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const InstallSnapshotResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}
// }}}
// {{{ RaftServer::MemoryStore
RaftServer::MemoryStore::MemoryStore()
    : isInitialized_(false),
      id_(),
      currentTerm_(),
      log_(),
      snapshottedTerm_(),
      snapshottedIndex_(),
      snapshotData_() {

  // log with index 0 is invalid. logs start with index 1
  log_.push_back(LogEntry());
}

bool RaftServer::MemoryStore::isInitialized() const {
  return isInitialized_;
}

void RaftServer::MemoryStore::initialize(Id id, Term term) {
  isInitialized_ = true;

  id_ = id;
  currentTerm_ = term;
  log_.clear();

  snapshottedTerm_ = 0;
  snapshottedIndex_ = 0;
  snapshotData_.clear();
}

RaftServer::Id RaftServer::MemoryStore::loadServerId() {
  return id_;
}

bool RaftServer::MemoryStore::saveTerm(Term currentTerm) {
  currentTerm_ = currentTerm;
  return true;
}

RaftServer::Term RaftServer::MemoryStore::loadTerm() {
  return currentTerm_;
}

bool RaftServer::MemoryStore::appendLogEntry(const LogEntry& log) {
  if (log_.size() != log.index()) {
    abort();
  }
  log_.push_back(log);
  return true;
}

void RaftServer::MemoryStore::loadLogEntry(Index index, LogEntry* log) {
  *log = log_[index];
}

bool RaftServer::MemoryStore::saveSnapshotBegin(Term currentTerm, Index lastIndex) {
  snapshottedTerm_ = currentTerm;
  snapshottedIndex_ = lastIndex;
  return true;
}

bool RaftServer::MemoryStore::saveSnapshotChunk(const uint8_t* data, size_t length) {
  // most ugly art to append a data block to another
  for (size_t i = 0; i < length; ++i) {
    snapshotData_.push_back(data[i]);
  }
  return true;
}

bool RaftServer::MemoryStore::saveSnapshotEnd() {
  return true;
}

bool RaftServer::MemoryStore::loadSnapshotBegin(Term* currentTerm, Index* lastIndex) {
  return false;
}

bool RaftServer::MemoryStore::loadSnapshotChunk(std::vector<uint8_t>* chunk) {
  return false;
}
// }}}

} // namespace xzero

