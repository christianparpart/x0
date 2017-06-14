// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/Buffer.h>
#include <cstdint>

namespace xzero {

class EndPointWriter;

namespace raft {

/**
 * @brief API for generating binary message frames for the Raft protocol.
 */
class Generator {
 public:
  typedef std::function<void(const uint8_t*, size_t)> ChunkWriter;

 public:
  explicit Generator(ChunkWriter writer);

  void generateHelloRequest(const HelloRequest& msg);
  void generateHelloResponse(const HelloResponse& msg);
  void generateVoteRequest(const VoteRequest& msg);
  void generateVoteResponse(const VoteResponse& msg);
  void generateAppendEntriesRequest(const AppendEntriesRequest& msg);
  void generateAppendEntriesResponse(const AppendEntriesResponse& msg);
  void generateInstallSnapshotRequest(const InstallSnapshotRequest& msg);
  void generateInstallSnapshotResponse(const InstallSnapshotResponse& msg);

 private:
  void fill(const uint8_t* data, size_t len);
  void flushFrame();

 private:
  ChunkWriter chunkWriter_;
  Buffer buffer_;
  BinaryWriter wire_;
};

} // namespace raft
} // namespace xzero
