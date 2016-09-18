// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/rpc.h>
#include <xzero/raft/Generator.h>
#include <xzero/net/EndPointWriter.h>

namespace xzero {
namespace raft {

Generator::Generator(EndPointWriter* output) : output_(output) {
}

void Generator::generateVoteRequest(const VoteRequest& msg) {
  generateFrameHeader(MessageType::VoteRequest,
                      sizeof(msg.term) +
                      sizeof(msg.candidateId) +
                      sizeof(msg.lastLogIndex) +
                      sizeof(msg.lastLogTerm));
  writeTerm(msg.term);
  writeId(msg.candidateId);
  writeIndex(msg.lastLogIndex);
  writeTerm(msg.lastLogTerm);
}

void Generator::generateVoteResponse(const VoteResponse& msg) {
  generateFrameHeader(MessageType::VoteResponse,
                      sizeof(msg.term) +
                      sizeof(msg.voteGranted));
  writeTerm(msg.term);
  writeFlag(msg.voteGranted);
}

void Generator::generateAppendEntriesRequest(const AppendEntriesRequest& msg) {
  size_t logEntriesSize = 0; // TODO
  generateFrameHeader(MessageType::AppendEntriesRequest,
                      sizeof(msg.term) +
                      sizeof(msg.leaderId) +
                      sizeof(msg.prevLogIndex) +
                      sizeof(msg.prevLogTerm) +
                      logEntriesSize +
                      sizeof(msg.leaderCommit));
}

void Generator::generateAppendEntriesResponse(const AppendEntriesResponse& msg) {
}

void Generator::generateInstallSnapshotRequest(const InstallSnapshotRequest& msg) {
}

void Generator::generateInstallSnapshotResponse(const InstallSnapshotResponse& msg) {
}

void Generator::generateFrameHeader(MessageType id, size_t payloadSize) {
}

} // namespace raft
} // namespace xzero
