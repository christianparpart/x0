// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/raft/Handler.h>
#include <xzero/raft/Transport.h>
#include <xzero/raft/Error.h>
#include <xzero/Option.h>
#include <xzero/DeadlineTimer.h>
#include <xzero/Duration.h>
#include <xzero/MonotonicTime.h>
#include <xzero/executor/Executor.h>
#include <xzero/thread/Wakeup.h>
#include <initializer_list>
#include <unordered_map>
#include <atomic>
#include <system_error>
#include <memory>
#include <list>

namespace xzero {
namespace raft {

enum class ServerState {
  Follower,
  Candidate,
  Leader,
};

typedef std::unordered_map<Id, Index> ServerIndexMap;

class Storage;
class Discovery;
class StateMachine;

/**
 * Provides a replicated state machine mechanism.
 */
class Server : public Handler {
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
         const Discovery* discovery,
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
         const Discovery* discovery,
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
  bool isFollower() const noexcept { return state_ == ServerState::Follower; }
  bool isCandidate() const noexcept { return state_ == ServerState::Candidate; }
  bool isLeader() const noexcept { return state_ == ServerState::Leader; }

  Storage* storage() const noexcept { return storage_; }
  const Discovery* discovery() const noexcept { return discovery_; }
  Transport* transport() const noexcept { return transport_; }

  size_t quorum() const;

  Term currentTerm() const;

  Id currentLeaderId() const;

  /**
   * Starts the Server.
   *
   * This starts the server initially in @c ServerState::Follower mode,
   * potentially timing out waiting for messages from the leader and then
   * triggering the leader election in @c ServerState::Candidate mode.
   */
  std::error_code start();

  /**
   * Starts the Server and assumes that @p leaderId is the current cluster leader.
   */
  std::error_code startWithLeader(Id leaderId);

  /**
   * Gracefully stops the server.
   *
   * If this server is a leader, it stops accepting new commands from clients,
   * ensure any pending commands have been replicated to the majority of
   * the cluster, and then steps back as leader.
   */
  void stop();

  /**
   * Sends given @p command to the Raft cluster.
   */
  std::error_code sendCommand(Command&& command);

  /**
   * Verifies whether or not this Server is (still) a #ServerState::Leader.
   *
   * This works by sending a heartbeat to all peers and count the replies.
   *
   * If the replies reach quorum, then pass @c true to the callback.
   *
   * If the leader just received a response from enough peers within
   * the last heartbeat time, we'll pass @c true right away, otherwise
   * @c false will be the passed result.
   *
   * @param result Future callback to be invoked as soon as leadership has been
   *               verified.
   *
   * @note If a leadership has been already verified within past
   *       electionTimeout, then the future callback will be invoked directly.
   */
  void verifyLeader(std::function<void(bool)> callback);

  // {{{ handler API (invoked by Transport on receiving messages)
  HelloResponse handleRequest(const HelloRequest& request) override;
  void handleResponse(Id from, const HelloResponse& response) override;
  VoteResponse handleRequest(Id from, const VoteRequest& request) override;
  void handleResponse(Id from, const VoteResponse& response) override;
  AppendEntriesResponse handleRequest(Id from, const AppendEntriesRequest& request) override;
  void handleResponse(Id from, const AppendEntriesResponse& response) override;
  InstallSnapshotResponse handleRequest(Id from, const InstallSnapshotRequest& request) override;
  void handleResponse(Id from, const InstallSnapshotResponse& response) override;
  // }}}

 private:
  void onTimeout();
  void sendVoteRequest();
  void setCurrentTerm(Term newTerm);
  void setState(ServerState newState);
  void setupLeader();
  void sendHeartbeat();
  Index latestIndex();
  Term getLogTerm(Index index);
  void replicateLogsTo(Id peerId);
  void applyLogs();
  MonotonicTime nextHeartbeat() const;

 private:
  Executor* executor_;
  Id id_;
  Id currentLeaderId_;
  Storage* storage_;
  const Discovery* discovery_;
  Transport* transport_;
  StateMachine* stateMachine_;
  ServerState state_;
  MonotonicTime nextHeartbeat_;
  DeadlineTimer timer_;

  std::list<std::function<void(bool)>> verifyLeaderCallbacks_;

  // ------------------- configuration ----------------------------------------
  Duration heartbeatTimeout_;
  Duration electionTimeout_;
  Duration commitTimeout_;
  unsigned maxCommandsPerMessage_; //!< number of commands to batch in an AppendEntriesRequest
  size_t maxMessageSize_;

  // ------------------- volatile state ---------------------------------------

  //! index of highest log entry known to be committed (initialized to 0,
  //! increases monotonically)
  Index commitIndex_;

  //! index of highest log entry applied to state machine (initialized to 0,
  //! increases monotonically)
  Index lastApplied_;

  // ------------------- volatile state on candidates -------------------------
  size_t votesGranted_;

  // ------------------- volatile state on leaders ----------------------------
  //! for each server, index of the next log entry
  //! to send to that server (initialized to leader
  //! last log index + 1)
  ServerIndexMap nextIndex_;

  //! for each server, index of highest log entry
  //! known to be replicated on server
  //! (initialized to 0, increases monotonically)
  ServerIndexMap matchIndex_;

  /*!
   * Timestamp of next heartbeat for each peer.
   */
  std::unordered_map<Id, MonotonicTime> nextHeartbeats_;

 public:
  std::function<void(Server* self, ServerState oldState)> onStateChanged;
  std::function<void(Id oldLeader)> onLeaderChanged;
};

} // namespace raft
} // namespace xzero

#include <xzero/raft/Server-inl.h>
