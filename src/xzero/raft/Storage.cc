// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Storage.h>
#include <xzero/executor/Executor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/File.h>
#include <xzero/io/MemoryMap.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
#include <xzero/Status.h>
#include <xzero/logging.h>
#include <stdlib.h>
#include <assert.h>

namespace xzero {
namespace raft {

// {{{ MemoryStore
MemoryStore::MemoryStore(Executor* executor)
    : executor_(executor),
      votedFor_(),
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
  logDebug("MemoryStore", "appendLogEntryAsync: at index:$0, $1", log_.size(), log);

  Promise<Index> promise;

  auto storeTask = [this, log, promise]() {
    log_.emplace_back(log);
    promise.success(latestIndex());
  };

  if (executor_) {
    // badly simulate disk speecs
    constexpr Duration delay = 50_milliseconds;
    executor_->executeAfter(delay, storeTask);
  } else {
    storeTask();
  }

  return promise.future();
}

Result<LogEntry> MemoryStore::getLogEntry(Index index) {
  // XXX we also support returning log[0] as this has a term of 0 and no command.
  //logDebug("MemoryStore", "getLogEntry: at $0/$1", index, latestIndex());
  //assert(index >= 0 && index <= latestIndex());

  if (index > latestIndex())
    return std::make_error_code(std::errc::result_out_of_range);

  return Result<LogEntry>(log_[index]);
}

void MemoryStore::truncateLog(Index last) {
  assert(last <= log_.size() && "Can only shrink the log array.");
  log_.resize(last);
}

std::error_code MemoryStore::saveSnapshot(std::unique_ptr<InputStream>&& state, Term term, Index lastIndex) {
  return makeErrorCode(Status::NotImplementedError);
}

std::error_code MemoryStore::loadSnapshot(std::unique_ptr<OutputStream>&& state, Term* term, Index* lastIndex) {
  return makeErrorCode(Status::NotImplementedError);
}
// }}}

// {{{ FileStore
FileStore::FileStore(const std::string& basedir)
  : basedir_(basedir),
    votedFor_(),
    latestIndex_(),
    currentTerm_(),
    logCache_(),
    indexToOffsetMapping_(),
    outputBuffer_() {
}

FileStore::~FileStore() {
}

Buffer FileStore::readFile(const std::string& name) {
  std::string path = FileUtil::joinPaths(basedir_, name);
  if (FileUtil::exists(path))
    return FileUtil::read(path);
  else
    return Buffer();
}

Option<std::pair<Id, Term>> FileStore::parseVote(const BufferRef& data) {
  BinaryReader in(data);
  auto id = in.tryParseVarUInt();
  auto term = in.tryParseVarUInt();

  if (id && term) {
    return Some(std::make_pair((Id) *id, (Term) *term));
  } else {
    return None();
  }
}

std::error_code FileStore::initialize(Id* id) {
  if (!FileUtil::isDirectory(basedir_)) {
    logDebug("raft.FileStore", "Creating directory $0", basedir_);
    FileUtil::mkdir_p(basedir_);
  }

  clusterId_ = readFile("cluster_id").str();
  serverId_ = readFile("server_id").toInt();
  votedFor_ = parseVote(readFile("voted_for"));

  // TODO FileUtil::joinPaths(basedir_, "log");

  return std::error_code();
}

Option<std::pair<Id, Term>> FileStore::votedFor() {
  return votedFor_;
}

std::error_code FileStore::clearVotedFor() {
  FileUtil::rm(FileUtil::joinPaths(basedir_, "voted_for"));
  votedFor_.reset();

  return std::error_code();
}

std::error_code FileStore::setVotedFor(Id id, Term term) {
  Buffer data;
  BinaryWriter writer(BufferUtil::writer(&data));
  writer.writeVarUInt(id);
  writer.writeVarUInt(term);

  votedFor_ = Some(std::make_pair(id, term));

  return std::error_code();
}

std::error_code FileStore::setCurrentTerm(Term currentTerm) {
  return makeErrorCode(Status::NotImplementedError);
}

Term FileStore::currentTerm() {
  return currentTerm_;
}

Index FileStore::latestIndex() {
  return latestIndex_;
}

std::error_code FileStore::appendLogEntry(const LogEntry& log) {
  Buffer data;
  {
    BinaryWriter writer(BufferUtil::writer(&data));
    writer.writeVarUInt(log.term());
    writer.writeVarUInt(log.type());
    writer.writeLengthDelimited(log.command().data(), log.command().size());
  }

  Buffer header;
  {
    BinaryWriter writer(BufferUtil::writer(&header));
    writer.writeFixed64(data.size());
    uint64_t crc = 0; // TODO
    writer.writeFixed64(crc);
  }

  logStream_->write(header.data(), header.size());
  logStream_->write(data.data(), data.size());

  return std::error_code();
}

Future<Index> FileStore::appendLogEntryAsync(const LogEntry& log) {
#if 0
  std::lock_guard<decltype(storeMutex_)> _lk(storeMutex_);

  storesPending_.emplace_back(log);
  const Index index = latestIndex + storesPending_.size();
  return storePromises_[index].future();
#endif
}

void FileStore::writePendingStores() {
#if 0
  std::list<LogEntry> pending;
  std::unordered_ma<Index, Promise<Index>> promises;

  {
    std::lock_guard<decltype(storeMutex_)> _lk(storeMutex_);
    pending = std::move(storesPending_);
    promises = std::move(storePromises_);
  }

  for (size_t i = 0; i < pending.size(); ++i) {
    writeLog(pending[i], &outputBuffer_);
  }

  outputBuffer_.clear();
  latestIndex_ += promises.size();
#endif
}

void FileStore::writeLoop() {
  for (;;) {
    writePendingStores();
  }
}

Result<LogEntry> FileStore::getLogEntry(Index index) {
  return makeErrorCode(Status::NotImplementedError);
}

void FileStore::truncateLog(Index last) {
}

std::error_code FileStore::saveSnapshot(std::unique_ptr<InputStream>&& state, Term term, Index lastIndex) {
  return makeErrorCode(Status::NotImplementedError);
}

std::error_code FileStore::loadSnapshot(std::unique_ptr<OutputStream>&& state, Term* term, Index* lastIndex) {
  return makeErrorCode(Status::NotImplementedError);
}
// }}}

} // namespace raft
} // namespace xzero
