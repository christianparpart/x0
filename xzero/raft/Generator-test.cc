// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/testing.h>
#include <xzero/BufferUtil.h>
#include <xzero/raft/Generator.h>

using namespace xzero;
using namespace xzero::raft;

TEST(raft_Generator, HelloRequest) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateHelloRequest(HelloRequest{ .serverId = 0x42,
                                       .psk = "psk" });
  EXPECT_EQ("\x06\x07\x42\x03psk", out);
}

TEST(raft_Generator, HelloResponse) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateHelloResponse(HelloResponse{ .success = true,
                                         .message = "pqr" });
  EXPECT_EQ("\x06\x08\x01\x03pqr", out);
}

TEST(raft_Generator, VoteRequest) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateVoteRequest(VoteRequest{ .term = 0x11,
                                     .candidateId = 0x22,
                                     .lastLogIndex = 0x33,
                                     .lastLogTerm = 0x44 });
  EXPECT_EQ("\x05\x01\x11\x22\x33\x44", out);
}

TEST(raft_Generator, VoteResponse) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateVoteResponse(VoteResponse{ .term = 0x11,
                                       .voteGranted = true });
  EXPECT_EQ("\x03\x02\x11\x01", out);
}

TEST(raft_Generator, AppendEntriesRequest) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateAppendEntriesRequest(AppendEntriesRequest{
      .term = 0x11,
      .leaderId = 0x12,
      .prevLogIndex = 0x13,
      .prevLogTerm = 0x14,
      .leaderCommit = 0x15,
      .entries = { LogEntry{0x11, Command{1, 2, 3, 4}},
                   LogEntry{0x11, Command{5, 6, 7, 8}} }});
  EXPECT_EQ(
      "\x15\x03\x11\x12\x13\x14\x15\x02\x11\x01\x04"
      "\x01\x02\x03\x04\x11\x01\x04\x05\x06\x07\x08", out);
}

TEST(raft_Generator, AppendEntriesResponse) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateAppendEntriesResponse(AppendEntriesResponse{
      .term = 2,
      .lastLogIndex = 3,
      .success = true });

  EXPECT_EQ("\x04\x04\x02\x3\x01", out);
}

TEST(raft_Generator, InstallSnapshotRequest) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateInstallSnapshotRequest(InstallSnapshotRequest{
      .term = 1,
      .leaderId = 2,
      .lastIncludedIndex = 3,
      .lastIncludedTerm = 4,
      .offset = 0,
      .data = {0x42, 0x13, 0x37},
      .done = true });

  EXPECT_EQ("\x0b\x05\x01\x02\x03\x04\x00\x03\x42\x13\x37\x01", out);
}

TEST(raft_Generator, InstallSnapshotResponse) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateInstallSnapshotResponse(InstallSnapshotResponse{ .term = 1 });

  EXPECT_EQ("\x02\x06\x01", out);
}
