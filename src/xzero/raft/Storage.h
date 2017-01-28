// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <cstdint>
#include <vector>
#include <memory>
#include <system_error>

namespace xzero {
namespace raft {

/**
 * Storage API that implements writing to and reading from
 * <b>persistent</b> store.
 */
class Storage {
 public:
  virtual ~Storage() {}

  /**
   * Initializes the underlying (persistend) storage.
   *
   * @param id the server's ID is stored at that address.
   * @param term the server's last term is stored at that address.
   *
   * @returns success if there was no error or an appropriate error code.
   */
  virtual std::error_code initialize(Id* id, Term* term) = 0;

  /**
   * Saves the given term as currentTerm to stable storage.
   *
   * @returns success if there was no error or an appropriate error code.
   */
  virtual std::error_code saveTerm(Term currentTerm) = 0;

  //! Returns the index of the last LogEntry or 0 if nothing written yet.
  virtual Index latestIndex() = 0;

  //! saves given LogEntry @p entry at the end of the current log.
  virtual bool appendLogEntry(const LogEntry& entry) = 0;

  //! loads the LogEntry from given @p index and stores it in @p log.
  virtual std::shared_ptr<LogEntry> getLogEntry(Index index) = 0;

  //! Deletes any log entry starting after @p last index.
  virtual void truncateLog(Index last) = 0;

  /**
   * Saves the snapshot @p state along with its latest @p term and @p lastIndex.
   */
  virtual bool saveSnapshot(std::unique_ptr<std::istream>&& state, Term term, Index lastIndex) = 0;

  /**
   * Loads a snapshot into @p state along with its latest @p term and @p lastIndex.
   *
   * @param state
   * @param term
   * @param lastIndex
   */
  virtual bool loadSnapshot(std::unique_ptr<std::ostream>&& state, Term* term, Index* lastIndex) = 0;
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
 public:
  MemoryStore();

  std::error_code initialize(Id* id, Term* term) override;
  std::error_code saveTerm(Term currentTerm) override;

  Index latestIndex() override;
  bool appendLogEntry(const LogEntry& log) override;
  std::shared_ptr<LogEntry> getLogEntry(Index index) override;
  void truncateLog(Index last) override;

  bool saveSnapshot(std::unique_ptr<std::istream>&& state, Term term, Index lastIndex) override;
  bool loadSnapshot(std::unique_ptr<std::ostream>&& state, Term* term, Index* lastIndex) override;

 private:
  Term currentTerm_;
  std::vector<std::shared_ptr<LogEntry>> log_;

  Term snapshottedTerm_;
  Index snapshottedIndex_;
  std::vector<uint8_t> snapshotData_;
};

class FileStore : public Storage {
 public:
  // TODO
};

} // namespace raft
} // namespace xzero
