// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/Buffer.h>
#include <xzero/StringUtil.h>
#include <fmt/format.h>
#include <string>
#include <vector>
#include <iosfwd>
#include <cstdint>

namespace xzero {
namespace raft {

typedef uint32_t Id;   // must not be 0
typedef uint64_t Term;
typedef uint64_t Index;

/**
 * The interface to the command that can modify the systems finite state machine.
 *
 * @see StateMachine
 */
typedef std::vector<uint8_t> Command;
typedef std::vector<uint8_t> Reply;

enum LogType {
  LOG_COMMAND = 1,
  LOG_PEER_ADD = 2,
  LOG_PEER_REMOVE = 3,
};

/**
 * A single immutable log entry in the log.
 */
class LogEntry {
 public:
  LogEntry(Term term, LogType type, Command cmd)
      : term_{term}, type_{type}, command_{std::move(cmd)} {}
  LogEntry(Term term, Command&& cmd)
      : LogEntry{term, LOG_COMMAND, std::move(cmd)} {}
  LogEntry(Term term, LogType type)
      : LogEntry{term, type, Command{}} {}
  LogEntry() : LogEntry{0, LOG_COMMAND, Command{}} {}

  LogEntry(const LogEntry&) = default;
  LogEntry& operator=(const LogEntry&) = default;

  LogEntry(LogEntry&&) = default;
  LogEntry& operator=(LogEntry&&) = default;

  Term term() const noexcept { return term_; }
  LogType type() const noexcept { return type_; }
  const Command& command() const { return command_; }

  bool isCommand(const BufferRef& cmd) const {
    if (command_.size() != cmd.size())
      return false;

    return memcmp(command_.data(), cmd.data(), cmd.size()) == 0;
  }

 private:
  Term term_;
  LogType type_;
  Command command_;
};

struct HelloRequest {
  Id serverId;              // my Server ID
  std::string psk;          // some pre-shared-key that must match across the cluster
};

struct HelloResponse {
  bool success;             // wether or not the peer welcomes you
  std::string message;      // a diagnostic message in case you're not welcome
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
  Index leaderCommit;            // leader's commitIndex
  std::vector<LogEntry> entries; // log entries to store (empty for heartbeat; may send more than one for efficiency)
};

struct AppendEntriesResponse {
  Term term;          // currentTerm, for the leader to update itself
  Index lastLogIndex; // follower's latest index
  bool success;       // true if follower contained entry matching prevLogIndex and prevLogTerm
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

namespace fmt {

  template<>
  struct formatter<xzero::raft::LogType> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::LogType& v, FormatContext &ctx) {
      switch (v) {
        case xzero::raft::LOG_COMMAND:
          return format_to(ctx.begin(), "LOG_COMMAND");
        case xzero::raft::LOG_PEER_ADD:
          return format_to(ctx.begin(), "LOG_PEER_ADD");
        case xzero::raft::LOG_PEER_REMOVE:
          return format_to(ctx.begin(), "LOG_PEER_REMOVE");
        default: {
          return format_to(ctx.begin(), "<{}>", (int) v);
        }
      }
    }
  };

  template<>
  struct formatter<xzero::raft::LogEntry> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::LogEntry& v, FormatContext &ctx) {
      if (v.type() == xzero::raft::LOG_COMMAND) {
        return format_to(ctx.begin(),
            "LogEntry<term:{}, command:{}>",
            v.term(),
            xzero::StringUtil::hexPrint(v.command().data(), v.command().size()));
      } else {
        return format_to(ctx.begin(),
            "LogEntry<term:{}, type:{}>",
            v.term(),
            v.type());
      }
    }
  };

  template<>
  struct formatter<xzero::raft::VoteResponse> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::VoteResponse& v, FormatContext &ctx) {
      return format_to(ctx.begin(), 
          "VoteResponse<term:{}, voteGranted:{}>",
          v.term,
          v.voteGranted);
    }
  };

  template<>
  struct formatter<xzero::raft::AppendEntriesRequest> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::AppendEntriesRequest& v, FormatContext &ctx) {
      return format_to(ctx.begin(), 
          "AppendEntriesRequest<term:{}, leaderId:{}, prevLogIndex:{}, prevLogTerm:{}, entries:{}, leaderCommit:{}>",
          v.term,
          v.leaderId,
          v.prevLogIndex,
          v.prevLogTerm,
          v.entries.size(),
          v.leaderCommit);
    }
  };

  template<>
  struct formatter<xzero::raft::AppendEntriesResponse> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::AppendEntriesResponse& v, FormatContext &ctx) {
      return format_to(ctx.begin(), 
          "AppendEntriesResponse<term:{}, lastLogIndex: {}, success:{}>",
          v.term,
          v.lastLogIndex,
          v.success);
    }
  };

  template<>
  struct formatter<xzero::raft::InstallSnapshotRequest> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::InstallSnapshotRequest& v, FormatContext &ctx) {
      return format_to(ctx.begin(), 
          "InstallSnapshotRequest<term:{}, leaderId:{}, lastIncludedIndex:{}, lastIncludedTerm:{}, offset:{}, dataSize:{}, done:{}>",
          v.term,
          v.leaderId,
          v.lastIncludedIndex,
          v.lastIncludedTerm,
          v.offset,
          v.data.size(),
          v.done);
    }
  };

  template<>
  struct formatter<xzero::raft::InstallSnapshotResponse> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::raft::InstallSnapshotResponse& v, FormatContext &ctx) {
      return format_to(ctx.begin(), "InstallSnapshotResponse<term:{}>", v.term);
    }
  };

}
