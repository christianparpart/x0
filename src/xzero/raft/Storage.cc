// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Storage.h>
#include <stdlib.h>
#include <assert.h>

namespace xzero {
namespace raft {

// {{{ AbstractStorage
std::shared_ptr<LogEntry> AbstractStorage::getLogTail() {
  if (Index index = latestIndex()) {
    return getLogEntry(index);
  } else {
    return nullptr;
  }
}
// }}}
// {{{ MemoryStore
MemoryStore::MemoryStore()
    : currentTerm_(),
      log_(),
      snapshottedTerm_(),
      snapshottedIndex_(),
      snapshotData_() {

  // log with index 0 is invalid. logs start with index 1
  log_.push_back(std::make_shared<LogEntry>());
}

std::error_code MemoryStore::initialize(Id* id, Term* term) {
  *term = currentTerm_;
  log_.resize(1);

  snapshottedTerm_ = 0;
  snapshottedIndex_ = 0;
  snapshotData_.clear();

  return std::error_code();
}

std::error_code MemoryStore::saveTerm(Term currentTerm) {
  currentTerm_ = currentTerm;
  return std::error_code();
}

Index MemoryStore::latestIndex() {
  return log_.size() - 1;
}

bool MemoryStore::appendLogEntry(const LogEntry& log) {
  log_.push_back(std::shared_ptr<LogEntry>(new LogEntry(log)));
  return true;
}

std::shared_ptr<LogEntry> MemoryStore::getLogEntry(Index index) {
  // XXX we also support returning log[0] as this has a term of 0 and no command.
  // assert(index >= 1 && index <= latestIndex());

  return log_[index];
}

void MemoryStore::truncateLog(Index last) {
  assert(last <= log_.size() && "Can only shrink the log array.");
  log_.resize(last);
}

bool MemoryStore::saveSnapshot(std::unique_ptr<std::istream>&& state, Term term, Index lastIndex) {
  return false;
}

bool MemoryStore::loadSnapshot(std::unique_ptr<std::ostream>&& state, Term* term, Index* lastIndex) {
  return false;
}
// }}}

} // namespace raft
} // namespace xzero
