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

namespace xzero {
namespace raft {

Generator::Generator(ChunkWriter writer)
  : wire_(writer) {
}

void Generator::generateVoteRequest(const VoteRequest& msg) {
  wire_.writeVarUInt((unsigned) MessageType::VoteRequest);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.candidateId);
  wire_.writeVarUInt(msg.lastLogIndex);
  wire_.writeVarUInt(msg.lastLogTerm);
}

void Generator::generateVoteResponse(const VoteResponse& msg) {
  printf("generateVoteResponse(term=%lu, granted=%d)!\n", msg.term, msg.voteGranted);
  wire_.writeVarUInt((unsigned) MessageType::VoteResponse);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.voteGranted ? 1 : 0);
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
}

void Generator::generateAppendEntriesResponse(const AppendEntriesResponse& msg) {
  wire_.writeVarUInt((unsigned) MessageType::AppendEntriesResponse);
  wire_.writeVarUInt(msg.term);
  wire_.writeVarUInt(msg.success);
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
}

void Generator::generateInstallSnapshotResponse(const InstallSnapshotResponse& msg) {
  wire_.writeVarUInt((unsigned) MessageType::InstallSnapshotResponse);
  wire_.writeVarUInt(msg.term);
}

} // namespace raft
} // namespace xzero
