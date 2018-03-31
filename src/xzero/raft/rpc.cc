// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/rpc.h>
#include <xzero/StringUtil.h>
#include <fmt/format.h>
#include <string.h>
#include <iostream>

namespace xzero {
namespace raft {

// {{{ LogEntry
LogEntry::LogEntry()
    : LogEntry(0) {
}

LogEntry::LogEntry(Term term)
    : LogEntry(term, Command()) {
}

LogEntry::LogEntry(Term term,
                   Command&& cmd)
    : LogEntry(term, LOG_COMMAND, std::move(cmd)) {
}

LogEntry::LogEntry(Term term,
                   LogType type)
    : LogEntry(term, type, Command()) {
}


LogEntry::LogEntry(Term term,
                   LogType type,
                   Command&& cmd)
    : term_(term),
      type_(type),
      command_(cmd) {
}

LogEntry::LogEntry(const LogEntry& v)
    : term_(v.term_),
      type_(v.type_),
      command_(v.command_) {
}

bool LogEntry::isCommand(const BufferRef& cmd) const {
  if (command_.size() != cmd.size())
    return false;

  return memcmp(command_.data(), cmd.data(), cmd.size()) == 0;
}

// }}}

std::ostream& operator<<(std::ostream& os, LogType value) {
  switch (value) {
    case LOG_COMMAND:
      return os << "LOG_COMMAND";
    case LOG_PEER_ADD:
      return os << "LOG_PEER_ADD";
    case LOG_PEER_REMOVE:
      return os << "LOG_PEER_REMOVE";
    default: {
      char buf[16];
      int n = snprintf(buf, sizeof(buf), "<%u>", value);
      return os << std::string(buf, n);
    }
  }
}

std::ostream& operator<<(std::ostream& os, const LogEntry& msg) {
  if (msg.type() == LOG_COMMAND) {
    return os << fmt::format(
        "LogEntry<term:{}, command:{}>",
        msg.term(),
        StringUtil::hexPrint(msg.command().data(), msg.command().size()));
  } else {
    return os << fmt::format(
        "LogEntry<term:{}, type:{}>",
        msg.term(),
        msg.type());
  }
}

std::ostream& operator<<(std::ostream& os, const VoteRequest& msg) {
  return os << fmt::format(
      "VoteRequest<term:{}, candidateId:{}, lastLogIndex:{}, lastLogTerm:{}>",
      msg.term,
      msg.candidateId,
      msg.lastLogIndex,
      msg.lastLogTerm);
}
std::ostream& operator<<(std::ostream& os, const VoteResponse& msg) {
  return os << fmt::format(
      "VoteResponse<term:{}, voteGranted:{}>",
      msg.term,
      msg.voteGranted);
}

std::ostream& operator<<(std::ostream& os, const AppendEntriesRequest& msg) {
  return os << fmt::format(
      "AppendEntriesRequest<term:{}, leaderId:{}, prevLogIndex:{}, prevLogTerm:{}, entries:{}, leaderCommit:{}>",
      msg.term,
      msg.leaderId,
      msg.prevLogIndex,
      msg.prevLogTerm,
      msg.entries.size(),
      msg.leaderCommit);
}

std::ostream& operator<<(std::ostream& os, const AppendEntriesResponse& msg) {
  return os << fmt::format(
      "AppendEntriesResponse<term:{}, lastLogIndex: {}, success:{}>",
      msg.term,
      msg.lastLogIndex,
      msg.success);
}

std::ostream& operator<<(std::ostream& os, const InstallSnapshotRequest& msg) {
  return os << fmt::format(
      "InstallSnapshotRequest<term:{}, leaderId:{}, lastIncludedIndex:{}, lastIncludedTerm:{}, offset:{}, dataSize:{}, done:{}>",
      msg.term,
      msg.leaderId,
      msg.lastIncludedIndex,
      msg.lastIncludedTerm,
      msg.offset,
      msg.data.size(),
      msg.done);
}

std::ostream& operator<<(std::ostream& os, const InstallSnapshotResponse& msg) {
  return os << fmt::format(
      "InstallSnapshotResponse<term:{}>",
      msg.term);
}

} // namespace raft
} // namespace xzero
