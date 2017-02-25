// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/thread/Future.h>
#include <xzero/io/InputStream.h>
#include <xzero/io/OutputStream.h>
#include <xzero/Option.h>
#include <xzero/Result.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <atomic>
#include <system_error>

namespace xzero {

class Executor;

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
   *
   * @returns success if there was no error or an appropriate error code.
   */
  virtual std::error_code initialize(Id* id) = 0;

  //! Clears the vote, iff any.
  virtual std::error_code clearVotedFor() = 0;

  //! Persists the given vote.
  virtual std::error_code setVotedFor(Id id, Term term) = 0;

  //! Candidate's Id that received vote in current term (or null if none).
  virtual Option<std::pair<Id, Term>> votedFor() = 0;

  /**
   * Saves the given term as currentTerm to stable storage.
   *
   * @returns success if there was no error or an appropriate error code.
   */
  virtual std::error_code setCurrentTerm(Term currentTerm) = 0;

  //! latest term server has seen (initialized to 0 on first boot,
  //! increases monotonically)
  virtual Term currentTerm() = 0;

  //! Returns the index of the last LogEntry or 0 if nothing written yet.
  virtual Index latestIndex() = 0;

  //! saves given LogEntry @p entry at the end of the current log.
  virtual std::error_code appendLogEntry(const LogEntry& entry) = 0;

  //! saves given LogEntry @p entry  at the end of the current log.
  virtual Future<Index> appendLogEntryAsync(const LogEntry& log) = 0;

  //! Retrieves the LogEntry from given @p index and stores it in @p log.
  //! retrieves log entry at given @p index.
  virtual Result<LogEntry> getLogEntry(Index index) = 0;

  //! Deletes any log entry starting after @p last index.
  virtual void truncateLog(Index last) = 0;

  /**
   * Saves the snapshot @p state along with its latest @p term and @p lastIndex.
   */
  virtual std::error_code saveSnapshot(std::unique_ptr<InputStream>&& state, Term term, Index lastIndex) = 0;

  /**
   * Loads a snapshot into @p state along with its latest @p term and @p lastIndex.
   *
   * @param state
   * @param term
   * @param lastIndex
   */
  virtual std::error_code loadSnapshot(std::unique_ptr<OutputStream>&& state, Term* term, Index* lastIndex) = 0;
};

/**
 * An in-memory based storage engine (use it only for testing!).
 *
 * This, of course, directly violates the paper. But this
 * implementation is very useful for testing.
 */
class MemoryStore : public Storage {
 public:
  explicit MemoryStore(Executor* executor);

  std::error_code initialize(Id* id) override;

  Option<std::pair<Id, Term>> votedFor() override;
  std::error_code clearVotedFor() override;
  std::error_code setVotedFor(Id id, Term term) override;

  std::error_code setCurrentTerm(Term currentTerm) override;
  Term currentTerm() override;
  Index latestIndex() override;
  std::error_code appendLogEntry(const LogEntry& log) override;
  Future<Index> appendLogEntryAsync(const LogEntry& log) override;
  Result<LogEntry> getLogEntry(Index index) override;
  void truncateLog(Index last) override;

  std::error_code saveSnapshot(std::unique_ptr<InputStream>&& state, Term term, Index lastIndex) override;
  std::error_code loadSnapshot(std::unique_ptr<OutputStream>&& state, Term* term, Index* lastIndex) override;

 private:
  Executor* executor_;

  Option<std::pair<Id, Term>> votedFor_;
  Term currentTerm_;
  std::vector<LogEntry> log_;

  Term snapshottedTerm_;
  Index snapshottedIndex_;
  std::vector<uint8_t> snapshotData_;
};

class FileStore : public Storage {
 public:
  explicit FileStore(const std::string& basedir);
  ~FileStore();

  std::error_code initialize(Id* id) override;

  Option<std::pair<Id, Term>> votedFor() override;
  std::error_code clearVotedFor() override;
  std::error_code setVotedFor(Id id, Term term) override;

  std::error_code setCurrentTerm(Term currentTerm) override;
  Term currentTerm() override;
  Index latestIndex() override;
  std::error_code appendLogEntry(const LogEntry& log) override;
  Future<Index> appendLogEntryAsync(const LogEntry& log) override;
  Result<LogEntry> getLogEntry(Index index) override;
  void truncateLog(Index last) override;

  std::error_code saveSnapshot(std::unique_ptr<InputStream>&& state, Term term, Index lastIndex) override;
  std::error_code loadSnapshot(std::unique_ptr<OutputStream>&& state, Term* term, Index* lastIndex) override;

 public: // helpers
  Buffer readFile(const std::string& filename);
  static Option<std::pair<Id, Term>> parseVote(const BufferRef& data);

  void writePendingStores();
  void writeLoop();

 private:
  std::string basedir_;
  std::unique_ptr<OutputStream> logStream_;

  // disk cache
  std::string clusterId_;
  Id serverId_;
  Option<std::pair<Id, Term>> votedFor_;
  std::atomic<Index> latestIndex_;
  Term currentTerm_;
  std::unordered_map<Index, LogEntry> logCache_;

  // async batched write
  std::mutex storeMutex_;
  std::list<LogEntry> storesPending_;
  std::unordered_map<Index, Promise<Index>> storePromises_;

  // read-helper: index-to-offset mapping
  std::unordered_map<Index, size_t> indexToOffsetMapping_;

  // write-helper: serialized log entries not yet flushed to disk
  Buffer outputBuffer_;
};

} // namespace raft
} // namespace xzero
