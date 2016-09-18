// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>

namespace xzero {

class EndPointWriter;

namespace raft {

/**
 * @brief API for generating binary message frames for the Raft protocol.
 */
class Generator {
 private:
  EndPointWriter* output_;

 public:
  explicit Generator(EndPointWriter* output);

  void generateVoteRequest(const VoteRequest& msg);
  void generateVoteResponse(const VoteResponse& msg);
  void generateAppendEntriesRequest(const AppendEntriesRequest& msg);
  void generateAppendEntriesResponse(const AppendEntriesResponse& msg);
  void generateInstallSnapshotRequest(const InstallSnapshotRequest& msg);
  void generateInstallSnapshotResponse(const InstallSnapshotResponse& msg);

 private:
  enum class MessageType {
    VoteRequest,
    VoteResponse,
    AppendEntriesRequest,
    AppendEntriesResponse,
    InstallSnapshotRequest,
    InstallSnapshotResponse,
  };

  void generateFrameHeader(MessageType id, size_t payloadSize);
  void writeTerm(Term term);
  void writeIndex(Index index);
  void writeId(Id id);
  void writeFlag(bool flag);
};

} // namespace raft
} // namespace xzero
