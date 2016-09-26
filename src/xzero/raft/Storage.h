// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/Option.h>
#include <cstdint>
#include <vector>
#include <memory>

namespace xzero {
namespace raft {

/**
 * Storage API that implements writing to and reading from
 * <b>persistent</b> store.
 */
class Storage {
 public:
  virtual ~Storage() {}

  //! Checks whether the underlying storage has been initialized already.
  virtual bool isInitialized() const = 0;

  //! Initializes the underlying storage (or resets it)
  virtual void initialize(Id id, Term term) = 0;

  virtual Id loadServerId() = 0;

  virtual bool saveTerm(Term currentTerm) = 0;
  virtual Term loadTerm() = 0;

  //! Returns the index of the last LogEntry or None if nothing written yet.
  virtual Option<Index> latestIndex() = 0;

  //! saves given LogEntry @p log.
  virtual bool appendLogEntry(const LogEntry& log) = 0;

  //! loads the LogEntry from given @p index and stores it in @p log.
  virtual std::shared_ptr<LogEntry> getLogEntry(Index index) = 0;

  virtual bool saveSnapshotBegin(Term currentTerm, Index lastIndex) = 0;
  virtual bool saveSnapshotChunk(const uint8_t* data, size_t length) = 0;
  virtual bool saveSnapshotEnd() = 0;

  virtual bool loadSnapshotBegin(Term* currentTerm, Index* lastIndex) = 0;
  virtual bool loadSnapshotChunk(std::vector<uint8_t>* chunk) = 0;
};

class AbstractStorage : public Storage {
 public:
  std::shared_ptr<LogEntry> getLogTail();
};

/**
 * An in-memory based storage engine (use it only for testing!).
 *
 * This, of course, directly violates the paper. But this
 * implementation is very useful for testing.
 */
class MemoryStore : public Storage {
 private:
  bool isInitialized_;
  Id id_;
  Term currentTerm_;
  std::vector<std::shared_ptr<LogEntry>> log_;

  Term snapshottedTerm_;
  Index snapshottedIndex_;
  std::vector<uint8_t> snapshotData_;

 public:
  MemoryStore();

  bool isInitialized() const override;
  void initialize(Id id, Term term) override;

  Id loadServerId() override;

  bool saveTerm(Term currentTerm) override;
  Term loadTerm() override;

  Option<Index> latestIndex() override;
  bool appendLogEntry(const LogEntry& log) override;
  std::shared_ptr<LogEntry> getLogEntry(Index index) override;

  bool saveSnapshotBegin(Term currentTerm, Index lastIndex) override;
  bool saveSnapshotChunk(const uint8_t* data, size_t length) override;
  bool saveSnapshotEnd() override;

  bool loadSnapshotBegin(Term* currentTerm, Index* lastIndex) override;
  bool loadSnapshotChunk(std::vector<uint8_t>* chunk) override;
};

class FileStore : public Storage {
 public:
  // TODO
};

} // namespace raft
} // namespace xzero
