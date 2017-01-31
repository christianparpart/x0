#include <xzero/testing.h>
#include <xzero/BufferUtil.h>
#include <xzero/raft/Generator.h>

using namespace xzero;
using namespace xzero::raft;

TEST(raft_Generator, VoteRequest) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateVoteRequest(VoteRequest{ .term = 0x11,
                                     .candidateId = 0x22,
                                     .lastLogIndex = 0x33,
                                     .lastLogTerm = 0x44 });
  //logf("VoteRequest: $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x01\x11\x22\x33\x44", out);
}

TEST(raft_Generator, VoteResponse) {
  Buffer out;
  raft::Generator g(BufferUtil::writer(&out));
  g.generateVoteResponse(VoteResponse{ .term = 0x11,
                                       .voteGranted = true });
  //logf("VoteResponse: $0", BufferUtil::hexPrint(&out));
  EXPECT_EQ("\x02\x11\x01", out);
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
  logf("AppendEntriesRequest: <$0>", BufferUtil::hexPrint(&out));
  //EXPECT_EQ("\x02\x11\x01", out);
}

TEST(raft_Generator, AppendEntriesResponse) {
}

TEST(raft_Generator, InstallSnapshotRequest) {
}

TEST(raft_Generator, InstallSnapshotResponse) {
}
