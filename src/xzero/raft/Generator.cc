// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/rpc.h>
#include <xzero/raft/Generator.h>
#include <xzero/net/EndPointWriter.h>

namespace xzero {
namespace raft {

protobuf::WireGenerator::ChunkWriter BufferWriter(Buffer* output) {
  return [output](const uint8_t* data, size_t len) {
    output->push_back(data, len);
  };
}

Generator::Generator(EndPointWriter* output)
  : buffer_(),
    wire_(BufferWriter(&buffer_)),
    output_(output) {
}

void Generator::generateVoteRequest(const VoteRequest& msg) {
  wire_.generateVarUInt((unsigned) MessageType::VoteRequest);
  wire_.generateVarUInt(msg.term);
  wire_.generateVarUInt(msg.candidateId);
  wire_.generateVarUInt(msg.lastLogIndex);
  wire_.generateVarUInt(msg.lastLogTerm);
}

void Generator::generateVoteResponse(const VoteResponse& msg) {
  wire_.generateVarUInt((unsigned) MessageType::VoteResponse);
  wire_.generateVarUInt(msg.term);
  wire_.generateVarUInt(msg.voteGranted);
}

void Generator::generateAppendEntriesRequest(const AppendEntriesRequest& msg) {
  wire_.generateVarUInt((unsigned) MessageType::AppendEntriesRequest);
  wire_.generateVarUInt(msg.entries.size());

  for (const auto& entry: msg.entries) {
    wire_.generateVarUInt(entry->term());
    wire_.generateVarUInt(entry->type());
    wire_.generateLengthDelimited(entry->command().data(),
                                  entry->command().size());
  }
}

void Generator::generateAppendEntriesResponse(const AppendEntriesResponse& msg) {
  wire_.generateVarUInt((unsigned) MessageType::AppendEntriesResponse);
  wire_.generateVarUInt(msg.term);
  wire_.generateVarUInt(msg.success);
}

void Generator::generateInstallSnapshotRequest(const InstallSnapshotRequest& msg) {
  wire_.generateVarUInt((unsigned) MessageType::InstallSnapshotRequest);

  wire_.generateVarUInt(msg.term);
  wire_.generateVarUInt(msg.leaderId);
  wire_.generateVarUInt(msg.lastIncludedIndex);
  wire_.generateVarUInt(msg.lastIncludedTerm);
  wire_.generateVarUInt(msg.offset);
  wire_.generateLengthDelimited(msg.data.data(), msg.data.size());
  wire_.generateVarUInt(msg.done);
}

void Generator::generateInstallSnapshotResponse(const InstallSnapshotResponse& msg) {
  wire_.generateVarUInt((unsigned) MessageType::InstallSnapshotResponse);
  wire_.generateVarUInt(msg.term);
}

void Generator::flushBuffer() {
  output_->write(std::move(buffer_));
}

} // namespace raft
} // namespace xzero
