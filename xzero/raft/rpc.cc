// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/rpc.h>
#include <xzero/StringUtil.h>
#include <string.h>

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

} // namespace raft

template<>
std::string StringUtil::toString(raft::LogType value) {
  switch (value) {
    case raft::LOG_COMMAND:
      return "LOG_COMMAND";
    case raft::LOG_PEER_ADD:
      return "LOG_PEER_ADD";
    case raft::LOG_PEER_REMOVE:
      return "LOG_PEER_REMOVE";
    default: {
      char buf[16];
      int n = snprintf(buf, sizeof(buf), "<%u>", value);
      return std::string(buf, n);
    }
  }
}

template<>
std::string StringUtil::toString(raft::LogEntry msg) {
  if (msg.type() == raft::LOG_COMMAND) {
    return StringUtil::format(
        "LogEntry<term:$0, command:$1>",
        msg.term(),
        StringUtil::hexPrint(msg.command().data(), msg.command().size()));
  } else {
    return StringUtil::format(
        "LogEntry<term:$0, type:$1>",
        msg.term(),
        msg.type());
  }
}

template<>
std::string StringUtil::toString(raft::VoteRequest msg) {
  return StringUtil::format(
      "VoteRequest<term:$0, candidateId:$1, lastLogIndex:$2, lastLogTerm:$3>",
      msg.term,
      msg.candidateId,
      msg.lastLogIndex,
      msg.lastLogTerm);
}

template<>
std::string StringUtil::toString(raft::VoteResponse msg) {
  return StringUtil::format(
      "VoteResponse<term:$0, voteGranted:$1>",
      msg.term,
      msg.voteGranted);
}

template<>
std::string StringUtil::toString(raft::AppendEntriesRequest msg) {
  return StringUtil::format(
      "AppendEntriesRequest<term:$0, leaderId:$1, prevLogIndex:$2, prevLogTerm:$3, entries:$4, leaderCommit:$5>",
      msg.term,
      msg.leaderId,
      msg.prevLogIndex,
      msg.prevLogTerm,
      msg.entries.size(),
      msg.leaderCommit);
}

template<>
std::string StringUtil::toString(raft::AppendEntriesResponse msg) {
  return StringUtil::format(
      "AppendEntriesResponse<term:$0, lastLogIndex: $1, success:$2>",
      msg.term,
      msg.lastLogIndex,
      msg.success);
}

template<>
std::string StringUtil::toString(raft::InstallSnapshotRequest msg) {
  return StringUtil::format(
      "InstallSnapshotRequest<term:$0, leaderId:$1, lastIncludedIndex:$2, lastIncludedTerm:$3, offset:$4, dataSize:$5, done:$6>",
      msg.term,
      msg.leaderId,
      msg.lastIncludedIndex,
      msg.lastIncludedTerm,
      msg.offset,
      msg.data.size(),
      msg.done);
}

template<>
std::string StringUtil::toString(raft::InstallSnapshotResponse msg) {
  return StringUtil::format(
      "InstallSnapshotResponse<term:$0>",
      msg.term);
}

} // namespace xzero
