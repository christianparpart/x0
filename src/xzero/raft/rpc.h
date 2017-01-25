// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace xzero {
namespace raft {

typedef uint32_t Id;   // must not be 0
typedef uint64_t Term;
typedef uint64_t Index;

enum class MessageType {
  RequestVote,
  AppendEntries,
  InstallSnapshot,
};

struct Request {
  MessageType requestType;
};

/**
 * The interface to the command that can modify the systems finite state machine.
 *
 * @see StateMachine
 */
typedef std::vector<uint8_t> Command;

enum LogType {
  LOG_COMMAND = 1,
  LOG_PEER_ADD = 2,
  LOG_PEER_REMOVE = 3,
};

/**
 * A single immutable log entry in the log.
 */
class LogEntry {
 private:
  LogEntry(Term term, LogType type, Command&& cmd);

 public:
  LogEntry(Term term, Command&& cmd);
  LogEntry(Term term, LogType type);
  LogEntry(Term term);
  LogEntry(const LogEntry& v);
  LogEntry();

  Term term() const noexcept { return term_; }
  LogType type() const noexcept { return type_; }
  const Command& command() const { return command_; }

 private:
  Term term_;
  LogType type_;
  Command command_;
};

// invoked by candidates to gather votes
struct VoteRequest {
  Term term;                // candidate's term
  Id candidateId;           // candidate requesting vode
  Index lastLogIndex;       // index of candidate's last log entry
  Term lastLogTerm;         // term of candidate's last log entry
};

struct VoteResponse {
  Term term;
  bool voteGranted;
};

// invoked by leader to replicate log entries; also used as heartbeat
struct AppendEntriesRequest {
  Term term;                     // leader's term
  Id leaderId;                   // so follower can redirect clients
  Index prevLogIndex;            // index of log entry immediately proceding new ones
  Term prevLogTerm;              // term of prevLogIndex entry
  std::vector<std::shared_ptr<LogEntry>> entries; // log entries to store (empty for heartbeat; may send more than one for efficiency)
  Index leaderCommit;            // leader's commitIndex
};

struct AppendEntriesResponse {
  Term term;    // currentTerm, for the leader to update itself
  bool success; // true if follower contained entry matching prevLogIndex and prevLogTerm
};

// Invoked by leader to send chunks of a snapshot to a follower.
// Leaders always send chunks in order.
struct InstallSnapshotRequest {
  Term term;
  Id leaderId;
  Index lastIncludedIndex;
  Term lastIncludedTerm;
  size_t offset;
  std::vector<uint8_t> data;
  bool done;
};

struct InstallSnapshotResponse {
  Term term;
};

} // namespace raft
} // namespace xzero
