// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/Parser.h>
#include <xzero/raft/Listener.h>
#include <xzero/testing.h>
#include <xzero/Buffer.h>

using namespace xzero;
using namespace xzero::raft;

class MessagePod : public raft::Listener { // {{{
 public:
  std::vector<HelloRequest> helloRequest;
  std::vector<HelloResponse> helloResponse;
  std::vector<VoteRequest> voteRequest;
  std::vector<VoteResponse> voteResponse;
  std::vector<AppendEntriesRequest> appendEntriesRequest;
  std::vector<AppendEntriesResponse> appendEntriesResponse;
  std::vector<InstallSnapshotRequest> installSnapshotRequest;
  std::vector<InstallSnapshotResponse> installSnapshotResponse;

  void receive(const HelloRequest& message) override {
    helloRequest.push_back(message);
  }

  void receive(const HelloResponse& message) override {
    helloResponse.push_back(message);
  }

  void receive(const VoteRequest& message) override {
    voteRequest.push_back(message);
  }

  void receive(const VoteResponse& message) override {
    voteResponse.push_back(message);
  }

  void receive(const AppendEntriesRequest& message) override {
    appendEntriesRequest.push_back(message);
  }

  void receive(const AppendEntriesResponse& message) override {
    appendEntriesResponse.push_back(message);
  }

  void receive(const InstallSnapshotRequest& message) override {
    installSnapshotRequest.push_back(message);
  }

  void receive(const InstallSnapshotResponse& message) override {
    installSnapshotResponse.push_back(message);
  }
};
// }}}

TEST(raft_Parser, HelloRequest) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x06\x07\x42\x03psk";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.helloRequest.size());
  EXPECT_EQ(0x42, pod.helloRequest[0].serverId);
  EXPECT_EQ("psk", pod.helloRequest[0].psk);
}

TEST(raft_Parser, HelloResponse) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x06\x08\x01\x03psk";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.helloResponse.size());
  EXPECT_TRUE(pod.helloResponse[0].success);
  EXPECT_EQ("psk", pod.helloResponse[0].message);
}

TEST(raft_Parser, VoteRequest) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x05\x01\x11\x22\x33\x44";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.voteRequest.size());
  EXPECT_EQ(0x11, pod.voteRequest[0].term);
  EXPECT_EQ(0x22, pod.voteRequest[0].candidateId);
  EXPECT_EQ(0x33, pod.voteRequest[0].lastLogIndex);
  EXPECT_EQ(0x44, pod.voteRequest[0].lastLogTerm);
}

TEST(raft_Parser, VoteResponse) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x02\x02\x03\x01";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.voteResponse.size());
  EXPECT_EQ(3, pod.voteResponse[0].term);
  EXPECT_TRUE(pod.voteResponse[0].voteGranted);
}

TEST(raft_Parser, AppendEntriesRequest) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x15\x03\x11\x12\x13\x14\x15\x02\x11\x01\x04"
                     "\x01\x02\x03\x04\x11\x01\x04\x05\x06\x07\x08";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.appendEntriesRequest.size());
  EXPECT_EQ(0x11, pod.appendEntriesRequest[0].term);
  EXPECT_EQ(0x12, pod.appendEntriesRequest[0].leaderId);
  EXPECT_EQ(0x13, pod.appendEntriesRequest[0].prevLogIndex);
  EXPECT_EQ(0x14, pod.appendEntriesRequest[0].prevLogTerm);
  EXPECT_EQ(0x15, pod.appendEntriesRequest[0].leaderCommit);
  ASSERT_EQ(2, pod.appendEntriesRequest[0].entries.size());

  EXPECT_EQ(0x11, pod.appendEntriesRequest[0].entries[0].term());
  EXPECT_TRUE(pod.appendEntriesRequest[0].entries[0].isCommand("\x01\x02\x03\x04"));
  EXPECT_EQ((int)raft::LOG_COMMAND, (int)pod.appendEntriesRequest[0].entries[0].type());

  EXPECT_EQ(0x11, pod.appendEntriesRequest[0].entries[1].term());
  EXPECT_TRUE(pod.appendEntriesRequest[0].entries[1].isCommand("\x05\x06\x07\x08"));
  EXPECT_EQ((int)raft::LOG_COMMAND, (int)pod.appendEntriesRequest[0].entries[1].type());
}

TEST(raft_Parser, AppendEntriesResponse) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x04\x04\x13\x17\x01";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.appendEntriesResponse.size());
  EXPECT_EQ(0x13, pod.appendEntriesResponse[0].term);
  EXPECT_EQ(0x17, pod.appendEntriesResponse[0].lastLogIndex);
  EXPECT_TRUE(pod.appendEntriesResponse[0].success);
}

TEST(raft_Parser, InstallSnapshotRequest) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x0b\x05\x01\x02\x03\x04\x00\x03\x42\x13\x37\x01";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.installSnapshotRequest.size());
  EXPECT_EQ(1, pod.installSnapshotRequest[0].term);
  EXPECT_EQ(2, pod.installSnapshotRequest[0].leaderId);
  EXPECT_EQ(3, pod.installSnapshotRequest[0].lastIncludedIndex);
  EXPECT_EQ(4, pod.installSnapshotRequest[0].lastIncludedTerm);
  EXPECT_EQ(0, pod.installSnapshotRequest[0].offset);
  EXPECT_EQ(3, pod.installSnapshotRequest[0].data.size());
  EXPECT_EQ(0x42, pod.installSnapshotRequest[0].data[0]);
  EXPECT_EQ(0x13, pod.installSnapshotRequest[0].data[1]);
  EXPECT_EQ(0x37, pod.installSnapshotRequest[0].data[2]);
  EXPECT_TRUE(pod.installSnapshotRequest[0].done);
}

TEST(raft_Parser, InstallSnapshotResponse) {
  MessagePod pod;
  raft::Parser parser(&pod);
  BufferRef binmsg = "\x02\x06\x13";

  unsigned parsedMessageCount = parser.parseFragment(binmsg);
  EXPECT_EQ(1, parsedMessageCount);
  ASSERT_EQ(1, pod.installSnapshotResponse.size());
  EXPECT_EQ(0x13, pod.installSnapshotResponse[0].term);
}

TEST(raft_Parser, read_partial) {
  MessagePod pod;
  raft::Parser parser(&pod);

  ASSERT_EQ(0, parser.parseFragment("\x02"));
  ASSERT_EQ(1, parser.pending());

  ASSERT_EQ(0, parser.parseFragment("\x06"));
  ASSERT_EQ(2, parser.pending());

  ASSERT_EQ(1, parser.parseFragment("\x13"));
  ASSERT_EQ(0, parser.pending());

  ASSERT_EQ(1, pod.installSnapshotResponse.size());
  EXPECT_EQ(0x13, pod.installSnapshotResponse[0].term);
}

TEST(raft_Parser, read_multi) {
  MessagePod pod;
  raft::Parser parser(&pod);

  unsigned parsedMessageCount =
      parser.parseFragment("\x02\x06\x13\x02\x06\x14\x02\x06\x15");

  ASSERT_EQ(3, parsedMessageCount);
  ASSERT_EQ(3, pod.installSnapshotResponse.size());
  EXPECT_EQ(0x13, pod.installSnapshotResponse[0].term);
  EXPECT_EQ(0x14, pod.installSnapshotResponse[1].term);
  EXPECT_EQ(0x15, pod.installSnapshotResponse[2].term);
}
