// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
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
    : isInitialized_(false),
      id_(),
      currentTerm_(),
      log_(),
      snapshottedTerm_(),
      snapshottedIndex_(),
      snapshotData_() {

  // log with index 0 is invalid. logs start with index 1
  log_.push_back(std::make_shared<LogEntry>());
}

bool MemoryStore::isInitialized() const {
  return isInitialized_;
}

void MemoryStore::initialize(Id id, Term term) {
  isInitialized_ = true;

  id_ = id;
  currentTerm_ = term;

  log_.resize(1);

  snapshottedTerm_ = 0;
  snapshottedIndex_ = 0;
  snapshotData_.clear();
}

Id MemoryStore::loadServerId() {
  return id_;
}

bool MemoryStore::saveTerm(Term currentTerm) {
  currentTerm_ = currentTerm;
  return true;
}

Term MemoryStore::loadTerm() {
  return currentTerm_;
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

bool MemoryStore::saveSnapshotBegin(Term currentTerm, Index lastIndex) {
  snapshottedTerm_ = currentTerm;
  snapshottedIndex_ = lastIndex;
  return true;
}

bool MemoryStore::saveSnapshotChunk(const uint8_t* data, size_t length) {
  // most ugly art to append a data block to another
  for (size_t i = 0; i < length; ++i) {
    snapshotData_.push_back(data[i]);
  }
  return true;
}

bool MemoryStore::saveSnapshotEnd() {
  return true;
}

bool MemoryStore::loadSnapshotBegin(Term* currentTerm, Index* lastIndex) {
  return false;
}

bool MemoryStore::loadSnapshotChunk(std::vector<uint8_t>* chunk) {
  return false;
}
// }}}

} // namespace raft
} // namespace xzero
