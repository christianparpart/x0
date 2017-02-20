// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Storage.h>
#include <xzero/logging.h>
#include <stdlib.h>
#include <assert.h>

namespace xzero {
namespace raft {

// {{{ MemoryStore
MemoryStore::MemoryStore()
    : votedFor_(),
      currentTerm_(1),
      log_(),
      snapshottedTerm_(),
      snapshottedIndex_(),
      snapshotData_() {

  // log with index 0 is invalid. logs start with index 1
  log_.push_back(LogEntry());
}

std::error_code MemoryStore::initialize(Id* id) {
  // *id = ...;
  log_.resize(1);

  snapshottedTerm_ = 0;
  snapshottedIndex_ = 0;
  snapshotData_.clear();

  return std::error_code();
}

std::error_code MemoryStore::clearVotedFor() {
  votedFor_ = None();
  return std::error_code();
}

std::error_code MemoryStore::setVotedFor(Id id, Term term) {
  votedFor_ = Some(std::make_pair(id, term));
  return std::error_code();
}

Option<std::pair<Id, Term>> MemoryStore::votedFor() {
  return votedFor_;
}

Term MemoryStore::currentTerm() {
  return currentTerm_;
}

std::error_code MemoryStore::setCurrentTerm(Term currentTerm) {
  currentTerm_ = currentTerm;
  return std::error_code();
}

Index MemoryStore::latestIndex() {
  return log_.size() - 1;
}

std::error_code MemoryStore::appendLogEntry(const LogEntry& log) {
  logDebug("MemoryStore", "appendLogEntry: at index:$0, $1", log_.size(), log);
  log_.emplace_back(log);
  return std::error_code();
}

Future<Index> MemoryStore::appendLogEntryAsync(const LogEntry& log) {
  logDebug("MemoryStore", "appendLogEntry: at index:$0, $1", log_.size(), log);

  Promise<Index> promise;

  // TODO: make it sleepy with executor_.executeAfter(some msecs) to emulate
  //       [slowish] disk speeds
  log_.emplace_back(log);
  promise.success(latestIndex());

  return promise.future();
}

Result<LogEntry> MemoryStore::getLogEntry(Index index) {
  // XXX we also support returning log[0] as this has a term of 0 and no command.
  //logDebug("MemoryStore", "getLogEntry: at $0/$1", index, latestIndex());
  //assert(index >= 0 && index <= latestIndex());

  if (index > latestIndex())
    return Failuref("No LogEntry at index $0", index);

  return Result<LogEntry>(log_[index]);
}

void MemoryStore::truncateLog(Index last) {
  assert(last <= log_.size() && "Can only shrink the log array.");
  log_.resize(last);
}

bool MemoryStore::saveSnapshot(std::unique_ptr<InputStream>&& state, Term term, Index lastIndex) {
  return false;
}

bool MemoryStore::loadSnapshot(std::unique_ptr<OutputStream>&& state, Term* term, Index* lastIndex) {
  return false;
}
// }}}

} // namespace raft
} // namespace xzero
