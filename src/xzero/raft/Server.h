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

class Executor;
class EndPoint;
class Connector;

namespace raft {

enum class ServerState {
  Follower,
  Candidate,
  Leader,
};

class Storage;
class Discovery;
class Transport;
class StateMachine;

/**
 * Provides a replicated state machine mechanism.
 */
class Server : public Listener {
 public:
  /**
   * its underlying storage engine @p storage and state machine,
   * with standard timeouts configured.
   *
   * @param id The Server's cluster-wide unique ID.
   * @param storage The underlying storage.
   * @param discovery Service Discovery, used to find peers.
   * @param transport Transport is used for peer communication.
   * @param stateMachine The finite state machine to apply the replication log's
   *                     commands onto.
   */
  Server(Executor* executor,
         Id id,
         Storage* storage,
         Discovery* discovery,
         Transport* transport,
         StateMachine* sm);

  /**
   * Initializes this log with given unique Id @p id,
   * its underlying storage engine @p storage and state machine @p sm.
   *
   * @param id The Server's cluster-wide unique ID.
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
   *                        this Server transitions into CANDIDATE state.
   * @param commitTimeout Time frame until when a commit into the StateMachine
   *                      must be be completed.
   */
  Server(Executor* executor,
         Id id,
         Storage* storage,
         Discovery* discovery,
         Transport* transport,
         StateMachine* stateMachine,
         Duration heartbeatTimeout,
         Duration electionTimeout,
         Duration commitTimeout);

  ~Server();

  Id id() const noexcept { return id_; }
  Index commitIndex() const noexcept { return commitIndex_; }
  Index lastApplied() const noexcept { return lastApplied_; }
  ServerState state() const noexcept { return state_; }
  LogEntry operator[](Index index);

  /**
   * Starts the Server.
   */
  void start();

  /**
   * Stops the server.
   */
  void stop();

  /**
   * Verifies whether or not this Server is (still) a @c LEADER.
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
  void onElectionTimeout();
  void setState(ServerState newState);

 private:
  Executor* executor_;
  Id id_;
  Storage* storage_;
  Discovery* discovery_;
  Transport* transport_;
  StateMachine* stateMachine_;
  ServerState state_;
  Random rng_;
  MonotonicTime nextHeartbeat_;
  std::list<std::function<void(bool)>> verifyLeaderCallbacks_;
  Executor::HandleRef electionTimeoutHandler_;

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

} // namespace raft
} // namespace xzero

#include <xzero/raft/Server-inl.h>
