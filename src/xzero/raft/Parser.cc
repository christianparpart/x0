// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Parser.h>
#include <xzero/raft/Listener.h>
#include <xzero/raft/MessageType.h>
#include <xzero/util/BinaryReader.h>

namespace xzero {
namespace raft {

Parser::Parser(Id myId, Listener* listener)
  : inputBuffer_(),
    inputOffset_(0),
    reader_(inputBuffer_),
    myId_(myId),
    listener_(listener) {
}

unsigned Parser::parseFragment(const uint8_t* chunk, size_t size) {
  inputBuffer_.push_back(chunk, size);
  unsigned messageCount = 0;

  while (parseFrame())
    messageCount++;

  return messageCount;
}

bool Parser::parseFrame() {
  reader_.reset(inputBuffer_.ref(inputOffset_));

  Option<uint64_t> payloadLen = reader_.tryParseVarUInt();
  if (payloadLen.isNone())
    return false;

  if (reader_.pending() < *payloadLen)
    return false;

  MessageType messageType = static_cast<MessageType>(reader_.parseVarUInt());

  switch (messageType) {
    case MessageType::VoteRequest:
      parseVoteRequest();
      break;
    case MessageType::VoteResponse:
      parseVoteResponse();
      break;
    case MessageType::AppendEntriesRequest:
      parseAppendEntriesRequest();
      break;
    case MessageType::AppendEntriesResponse:
      parseAppendEntriesResponse();
      break;
    case MessageType::InstallSnapshotRequest:
      parseInstallSnapshotRequest();
      break;
    case MessageType::InstallSnapshotResponse:
      parseInstallSnapshotResponse();
      break;
    default:
      RAISE(ProtocolError, "Invalid message type.");
  }

  if (reader_.pending() > 0) {
    inputOffset_ = inputBuffer_.size() - reader_.pending();
  } else {
    inputBuffer_.clear();
    inputOffset_ = 0;
  }

  return true;
}

void Parser::parseVoteRequest() {
  Term term = static_cast<Term>(reader_.parseVarUInt());
  Id candidateId = static_cast<Id>(reader_.parseVarUInt());
  Index lastLogIndex = static_cast<Index>(reader_.parseVarUInt());
  Term lastLogTerm = static_cast<Term>(reader_.parseVarUInt());

  listener_->receive(myId_, VoteRequest{ .term = term,
                                         .candidateId = candidateId,
                                         .lastLogIndex = lastLogIndex,
                                         .lastLogTerm = lastLogTerm });
}

void Parser::parseVoteResponse() {
  Term term = static_cast<Term>(reader_.parseVarUInt());
  bool voteGranted = static_cast<bool>(reader_.parseVarUInt());

  listener_->receive(myId_, VoteResponse{ .term = term,
                                          .voteGranted = voteGranted });
}

void Parser::parseAppendEntriesRequest() {
  Term term = static_cast<Term>(reader_.parseVarUInt());
  Id leaderId = static_cast<Id>(reader_.parseVarUInt());
  Index prevLogIndex = static_cast<Index>(reader_.parseVarUInt());
  Term prevLogTerm = static_cast<Term>(reader_.parseVarUInt());
  Index leaderCommit = static_cast<Index>(reader_.parseVarUInt());
  size_t entryCount = static_cast<size_t>(reader_.parseVarUInt());

  std::vector<LogEntry> entries;
  for (size_t i = 0; i < entryCount; ++i) {
    Term term = static_cast<Term>(reader_.parseVarUInt());
    LogType type = static_cast<LogType>(reader_.parseVarUInt());
    Command command = (Command) reader_.parseLengthDelimited();
    entries.emplace_back(term, type, std::move(command));
  }

  listener_->receive(myId_, AppendEntriesRequest{
      term,
      leaderId,
      prevLogIndex,
      prevLogTerm,
      leaderCommit,
      std::move(entries)});
}

void Parser::parseAppendEntriesResponse() {
  Term term = static_cast<Term>(reader_.parseVarUInt());
  bool success = static_cast<bool>(reader_.parseVarUInt());

  listener_->receive(myId_, AppendEntriesResponse{term, success});
}

void Parser::parseInstallSnapshotRequest() {
  Term term = static_cast<Term>(reader_.parseVarUInt());
  Id leaderId = static_cast<Id>(reader_.parseVarUInt());
  Index lastIncludedIndex = static_cast<Index>(reader_.parseVarUInt());
  Term lastIncludedTerm = static_cast<Term>(reader_.parseVarUInt());
  size_t offset = static_cast<size_t>(reader_.parseVarUInt());
  std::vector<uint8_t> data = reader_.parseLengthDelimited();
  bool done = static_cast<bool>(reader_.parseVarUInt());

  listener_->receive(myId_, InstallSnapshotRequest{
      term,
      leaderId,
      lastIncludedIndex,
      lastIncludedTerm,
      offset,
      std::move(data),
      done});
}

void Parser::parseInstallSnapshotResponse() {
  Term term = static_cast<Term>(reader_.parseVarUInt());

  listener_->receive(myId_, InstallSnapshotResponse{term});
}

} // namespace raft
} // namespace xzero
