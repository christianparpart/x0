// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/raft/Listener.h>
#include <xzero/Option.h>
#include <xzero/Duration.h>
#include <xzero/Random.h>
#include <xzero/MonotonicTime.h>
#include <xzero/executor/Executor.h>
#include <xzero/net/Server.h>
#include <initializer_list>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <list>

namespace xzero {

class EndPoint;
class Connector;

/**
 * Provides a replicated state machine mechanism.
 */
class RaftServer : public RaftListener {
 public:
  /**
   * Abstracts the systems state machine.
   */
  class StateMachine {
   public:
    virtual ~StateMachine() {}

    virtual void loadSnapshotBegin() = 0;
    virtual void loadSnapshotChunk(const std::vector<uint8_t>& chunk) = 0;
    virtual void loadSnapshotEnd() = 0;

    virtual void applyCommand(const Command& command) = 0;
  };

  /**
   * Abstracts communication between RaftServer instances.
   */
  class Transport {
   public:
    virtual ~Transport() {}

    // leader
    virtual void send(Id target, const VoteRequest& message) = 0;
    virtual void send(Id target, const AppendEntriesRequest& message) = 0;
    virtual void send(Id target, const InstallSnapshotRequest& message) = 0;

    // follower / candidate
    virtual void send(Id target, const AppendEntriesResponse& message) = 0;
    virtual void send(Id target, const VoteResponse& message) = 0;
    virtual void send(Id target, const InstallSnapshotResponse& message) = 0;
  };
  class LocalTransport;
  class InetTransport;

  /**
   * API for discovering cluster members.
   */
  class Discovery {
   public:
    virtual ~Discovery() {}

    /**
     * Retrieves a list of all candidates in a cluster by their Id.
     */
    virtual std::vector<Id> listMembers() = 0;
  };

  class StaticDiscovery;
  class DnsDiscovery;

  enum LogType {
    LOG_COMMAND,
    LOG_PEER_ADD,
    LOG_PEER_REMOVE,
  };

  /**
   * A single log entry in the log.
   */
  class LogEntry {
   private:
    LogEntry(Term term, Index index, LogType type, Command&& cmd);
   public:
    LogEntry(Term term, Index index, Command&& cmd);
    LogEntry(Term term, Index index, LogType type);
    LogEntry(Term term, Index index);
    LogEntry();

    Term term() const noexcept { return term_; }
    Index index() const noexcept { return index_; }
    LogType type() const noexcept { return type_; }

    const Command& command() const { return command_; }
    Command& command() { return command_; }

   private:
    Term term_;
    Index index_;
    LogType type_;
    Command command_;
  };

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

    //! saves given LogEntry @p log.
    virtual bool appendLogEntry(const LogEntry& log) = 0;

    //! loads the LogEntry from given @p index and stores it in @p log.
    virtual void loadLogEntry(Index index, LogEntry* log) = 0;

    virtual bool saveSnapshotBegin(Term currentTerm, Index lastIndex) = 0;
    virtual bool saveSnapshotChunk(const uint8_t* data, size_t length) = 0;
    virtual bool saveSnapshotEnd() = 0;

    virtual bool loadSnapshotBegin(Term* currentTerm, Index* lastIndex) = 0;
    virtual bool loadSnapshotChunk(std::vector<uint8_t>* chunk) = 0;
  };
  class MemoryStore;
  class FileStore;

  enum State {
    FOLLOWER,
    CANDIDATE,
    LEADER,
  };

 public:
  /**
   * its underlying storage engine @p storage and state machine,
   * with standard timeouts configured.
   *
   * @param id The RaftServer's cluster-wide unique ID.
   * @param storage The underlying storage.
   * @param discovery Service Discovery, used to find peers.
   * @param transport Transport is used for peer communication.
   * @param stateMachine The finite state machine to apply the replication log's
   *                     commands onto.
   */
  RaftServer(Executor* executor,
             Id id,
             Storage* storage,
             Discovery* discovery,
             Transport* transport,
             StateMachine* sm);

  /**
   * Initializes this log with given unique Id @p id,
   * its underlying storage engine @p storage and state machine @p sm.
   *
   * @param id The RaftServer's cluster-wide unique ID.
   * @param storage The underlying storage.
   * @param discovery Service Discovery, used to find peers.
   * @param transport Transport is used for peer communication.
   * @param stateMachine The finite state machine to apply the replication log's
   *                     commands onto.
   * @param heartbeatTimeout TODO
   * @param electionTimeout Time frame within at least one message must have
   *                        been arrived from the current leader.
   *                        If no message has been arrived within this
   *                        time frame, the leader is considered dead and
   *                        this RaftServer transitions into CANDIDATE state.
   * @param commitTimeout Time frame until when a commit into the StateMachine
   *                      must be be completed.
   */
  RaftServer(Executor* executor,
             Id id,
             Storage* storage,
             Discovery* discovery,
             Transport* transport,
             StateMachine* stateMachine,
             Duration heartbeatTimeout,
             Duration electionTimeout,
             Duration commitTimeout);

  ~RaftServer();

  Id id() const noexcept { return id_; }
  Index commitIndex() const noexcept { return commitIndex_; }
  Index lastApplied() const noexcept { return lastApplied_; }
  State state() const noexcept { return state_; }
  LogEntry operator[](Index index);

  /**
   * Starts the RaftServer.
   */
  void start();

  /**
   * Stops the server.
   */
  void stop();

  /**
   * Verifies whether or not this RaftServer is (still) a @c LEADER.
   *
   * @param result Future callback to be invoked as soon as leadership has been
   *               verified.
   *
   * @note If a leadership has been already verified within past
   *       electionTimeout, then the future callback will be invoked directly.
   */
  void verifyLeader(std::function<void(bool)> callback);

  // {{{ receiver API (invoked by Transport on receiving messages)
  // leader
  void receive(Id from, const AppendEntriesResponse& message);
  void receive(Id from, const InstallSnapshotResponse& message);

  // candidate
  void receive(Id from, const VoteRequest& message);
  void receive(Id from, const VoteResponse& message);

  // candidate / follower
  void receive(Id from, const AppendEntriesRequest& message);

  // follower
  void receive(Id from, const InstallSnapshotRequest& message);
  // }}}

 private:
  Duration varyingElectionTimeout();

 private:
  Executor* executor_;
  Id id_;
  Storage* storage_;
  Discovery* discovery_;
  Transport* transport_;
  StateMachine* stateMachine_;
  State state_;
  Random rng_;
  MonotonicTime nextHeartbeat_;
  std::list<std::function<void(bool)>> verifyLeaderCallbacks_;

  // ------------------- configuration ----------------------------------------
  Duration heartbeatTimeout_;
  Duration electionTimeout_;
  Duration commitTimeout_;

  // ------------------- persistet state --------------------------------------

  //! latest term server has seen (initialized to 0 on first boot,
  //! increases monotonically)
  Term currentTerm_;

  //! candidate's Id that received vote in current term (or null if none)
  Option<Id> votedFor_;

  //! log entries; each entry contains command for state machine,
  //! and term when entry was received by leader (first index is 1)
  //std::vector<LogEntry> log_;

  // ------------------- volatile state ---------------------------------------

  //! index of highest log entry known to be committed (initialized to 0,
  //! increases monotonically)
  Index commitIndex_;

  //! index of highest log entry applied to state machine (initialized to 0,
  //! increases monotonically)
  Index lastApplied_;

  // ------------------- volatile state on leaders ----------------------------

  //! for each server, index of the next log entry
  //! to send to that server (initialized to leader
  //! last log index + 1)
  std::vector<Index> nextIndex_;

  //! for each server, index of highest log entry
  //! known to be replicated on server
  //! (initialized to 0, increases monotonically)
  std::vector<Index> matchIndex_;
};

/**
 * API to static service discovery for the RaftServer.
 */
class RaftServer::StaticDiscovery : public Discovery {
 public:
  StaticDiscovery();
  StaticDiscovery(std::initializer_list<Id>&& list);

  void add(Id id);

  std::vector<Id> listMembers() override;

 private:
  std::vector<Id> members_;
};

/**
 * Implements DNS based service discovery that honors SRV records,
 * and if none available, A records.
 */
class RaftServer::DnsDiscovery : public Discovery {
 public:
  DnsDiscovery(const std::string& fqdn);
  ~DnsDiscovery();

  std::vector<Id> listMembers() override;
};

/**
 * An in-memory based storage engine (use it only for testing!).
 *
 * This, of course, directly violates the paper. But this
 * implementation is very useful for testing.
 */
class RaftServer::MemoryStore : public Storage {
 private:
  bool isInitialized_;
  Id id_;
  Term currentTerm_;
  std::vector<LogEntry> log_;

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

  bool appendLogEntry(const LogEntry& log) override;
  void loadLogEntry(Index index, LogEntry* log) override;

  bool saveSnapshotBegin(Term currentTerm, Index lastIndex) override;
  bool saveSnapshotChunk(const uint8_t* data, size_t length) override;
  bool saveSnapshotEnd() override;

  bool loadSnapshotBegin(Term* currentTerm, Index* lastIndex) override;
  bool loadSnapshotChunk(std::vector<uint8_t>* chunk) override;
};

/**
 * An on-disk based storage engine.
 */
class RaftServer::FileStore : public Storage {
  // TODO
};

class RaftServer::LocalTransport : public Transport {
 public:
  explicit LocalTransport(Id localId);

  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const VoteResponse& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const AppendEntriesResponse& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;
  void send(Id target, const InstallSnapshotResponse& message) override;

 private:
  RaftServer::Id localId_;
  std::unordered_map<Id, RaftServer*> peers_;
};

class RaftServer::InetTransport : public Transport {
 public:
  InetTransport(RaftServer* receiver);
  ~InetTransport();

  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const VoteResponse& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const AppendEntriesResponse& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;
  void send(Id target, const InstallSnapshotResponse& message) override;

 private:
  RaftServer* receiver_;
  std::unique_ptr<Connector> connector_;
  std::unordered_map<Id, EndPoint*> endpoints_;
};

} // namespace xzero

#include <xzero/RaftServer-inl.h>
