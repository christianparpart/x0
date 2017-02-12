// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/rpc.h>
#include <xzero/raft/Generator.h>
#include <xzero/raft/MessageType.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/BufferUtil.h>

namespace xzero {
namespace raft {

Generator::Generator(ChunkWriter writer)
  : chunkWriter_(writer),
    buffer_(),
    wire_(std::bind(&Generator::fill, this,
                    std::placeholders::_1,
                    std::placeholders::_2)) {
}

void Generator::fill(const uint8_t* data, size_t len) {
  buffer_.push_back(data, len);
}

void Generator::flushFrame() {
  Buffer frame;
  BinaryWriter(BufferUtil::writer(&frame)).writeVarUInt(buffer_.size());
  chunkWriter_((const uint8_t*) frame.data(), frame.size());
  chunkWriter_((const uint8_t*) buffer_.data(), buffer_.size());
  buffer_.clear();
}

void Generator::generateHelloRequest(const HelloRequest& msg) {
  wire_.writeVarUInt((unsigned) MessageType::HelloRequest);
  wire_.writeVarUInt(msg.serverId);
  wire_.writeString(msg.psk);
  flushFrame();
}

void Generator::generateHelloResponse(const HelloResponse& msg) {
  wire_.writeVarUInt((unsigned) MessageType::HelloResponse);
  wire_.writeVarUInt(msg.success ? 1 : 0);
  wire_.writeString(msg.message);
  flushFrame();
}

void Generator::generateVoteRequest(const VoteRequest& msg) {
  wire_.writeVarUInt((unsigned) MessageType::VoteRequest);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.candidateId);
  wire_.writeVarUInt(msg.lastLogIndex);
  wire_.writeVarUInt(msg.lastLogTerm);
  flushFrame();
}

void Generator::generateVoteResponse(const VoteResponse& msg) {
  wire_.writeVarUInt((unsigned) MessageType::VoteResponse);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.voteGranted ? 1 : 0);
  flushFrame();
}

void Generator::generateAppendEntriesRequest(const AppendEntriesRequest& msg) {
  wire_.writeVarUInt((unsigned) MessageType::AppendEntriesRequest);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.leaderId);
  wire_.writeVarUInt(msg.prevLogIndex);
  wire_.writeVarUInt(msg.prevLogTerm);
  wire_.writeVarUInt(msg.leaderCommit);
  wire_.writeVarUInt(msg.entries.size());

  for (const auto& entry: msg.entries) {
    wire_.writeVarUInt(entry.term());
    wire_.writeVarUInt(entry.type());
    wire_.writeLengthDelimited(entry.command().data(),
                               entry.command().size());
  }
  flushFrame();
}

void Generator::generateAppendEntriesResponse(const AppendEntriesResponse& msg) {
  wire_.writeVarUInt((unsigned) MessageType::AppendEntriesResponse);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.lastLogIndex);
  wire_.writeVarUInt(msg.success ? 1 : 0);
  flushFrame();
}

void Generator::generateInstallSnapshotRequest(const InstallSnapshotRequest& msg) {
  wire_.writeVarUInt((unsigned) MessageType::InstallSnapshotRequest);

  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.leaderId);
  wire_.writeVarUInt(msg.lastIncludedIndex);
  wire_.writeVarUInt(msg.lastIncludedTerm);
  wire_.writeVarUInt(msg.offset);
  wire_.writeLengthDelimited(msg.data.data(), msg.data.size());
  wire_.writeVarUInt(msg.done);
  flushFrame();
}

void Generator::generateInstallSnapshotResponse(const InstallSnapshotResponse& msg) {
  wire_.writeVarUInt((unsigned) MessageType::InstallSnapshotResponse);
  wire_.writeVarUInt(msg.term);
  flushFrame();
}

} // namespace raft
} // namespace xzero
