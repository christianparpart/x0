// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/ServerUtil.h>
#include <xzero/testing.h>

using namespace xzero;

TEST(raft_ServerUtil, majorityIndexOf) {
  raft::ServerIndexMap m1 {
    { 1, 7 },
  };
  EXPECT_EQ(7, raft::ServerUtil::majorityIndexOf(m1));

  raft::ServerIndexMap m2 {
    { 1, 6 },
    { 1, 8 },
  };
  EXPECT_EQ(6, raft::ServerUtil::majorityIndexOf(m2));

  raft::ServerIndexMap m3 {
    { 1, 1 },
    { 2, 2 }, // 2
    { 3, 3 }, // 2
  };
  EXPECT_EQ(2, raft::ServerUtil::majorityIndexOf(m3));

  raft::ServerIndexMap m4 {
    { 1, 1 },
    { 2, 2 }, // 2
    { 3, 3 }, // 2
    { 4, 4 }, // 2
  };
  EXPECT_EQ(2, raft::ServerUtil::majorityIndexOf(m4));

  raft::ServerIndexMap m5 {
    { 1, 1 },
    { 2, 2 },
    { 3, 3 },
    { 4, 4 },
    { 5, 5 },
  };
  EXPECT_EQ(3, raft::ServerUtil::majorityIndexOf(m5));

  raft::ServerIndexMap m5b {
    { 1, 1 },
    { 2, 2 },
    { 3, 4 },
    { 4, 4 },
    { 5, 5 },
  };
  EXPECT_EQ(4, raft::ServerUtil::majorityIndexOf(m5b));
}
