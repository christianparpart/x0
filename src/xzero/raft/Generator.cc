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

const size_t SizeTerm = sizeof(uint32_t);
//...

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

size_t sizeOfLogEntries(const std::vector<std::shared_ptr<LogEntry>>& entries) {
  size_t total = 0;

  for (const auto& entry: entries) {
    total += sizeof(uint64_t) + // term
             sizeof(uint8_t) +  // log type
             sizeof(uint64_t) + // command size encoded
             entry->command().size();
  }

  return total;
}

void Generator::generateAppendEntriesRequest(const AppendEntriesRequest& msg) {
  generateFrameHeader(MessageType::AppendEntriesRequest,
                      sizeof(msg.term) +
                      sizeof(msg.leaderId) +
                      sizeof(msg.prevLogIndex) +
                      sizeof(msg.prevLogTerm) +
                      sizeOfLogEntries(msg.entries) +
                      sizeof(msg.leaderCommit));
  for (const auto& entry: msg.entries) {
    writeTerm(entry->term());
    write8(entry->type());
    writeByteArray(entry->command());
  }
}

void Generator::generateAppendEntriesResponse(const AppendEntriesResponse& msg) {
  generateFrameHeader(MessageType::AppendEntriesResponse, 8 + 1);
  writeTerm(msg.term);
  writeFlag(msg.success);
}

void Generator::generateInstallSnapshotRequest(const InstallSnapshotRequest& msg) {
  generateFrameHeader(MessageType::InstallSnapshotRequest,
                      8 + 4 + 8 + 8 + 8 + 8 + msg.data.size() + 1);

  writeTerm(msg.term);                // 8
  writeId(msg.leaderId);              // 4
  writeIndex(msg.lastIncludedIndex);  // 8
  writeTerm(msg.lastIncludedTerm);    // 8
  writeSize(msg.offset);              // 8
  writeByteArray(msg.data);           // 8 + len(data)
  writeFlag(msg.done);                // 1
}

void Generator::generateInstallSnapshotResponse(const InstallSnapshotResponse& msg) {
  generateFrameHeader(MessageType::InstallSnapshotResponse, 8);
  writeTerm(msg.term);
}

void Generator::generateFrameHeader(MessageType id, size_t payloadSize) {
  write8(static_cast<uint8_t>(id));
  write64(payloadSize);
}

void Generator::writeTerm(Term term) {
  write64(term);
}

void Generator::writeIndex(Index index) {
  write64(index);
}

void Generator::writeId(Id id) {
  write32(id);
}

void Generator::writeFlag(bool value) {
  write8(value ? 1 : 0);
}

void Generator::writeSize(size_t value) {
  write64(value);
}

void Generator::write64(uint64_t value) {
  write8((value >> 56) & 0xFF);
  write8((value >> 48) & 0xFF);
  write8((value >> 40) & 0xFF);
  write8((value >> 32) & 0xFF);

  write8((value >> 24) & 0xFF);
  write8((value >> 16) & 0xFF);
  write8((value >> 8) & 0xFF);
  write8((value >> 0) & 0xFF);
}

void Generator::write32(uint64_t value) {
  write8((value >> 24) & 0xFF);
  write8((value >> 16) & 0xFF);
  write8((value >> 8) & 0xFF);
  write8((value >> 0) & 0xFF);
}

void Generator::write8(uint8_t value) {
  buffer_.push_back(&value, sizeof(value));
}

void Generator::writeByteArray(const std::vector<uint8_t>& data) {
  write64(data.size());
  buffer_.push_back(data.data(), data.size());
}

} // namespace raft
} // namespace xzero
