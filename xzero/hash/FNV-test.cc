// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/hash/FNV.h>

TEST(FNV, FNV64) {
  xzero::hash::FNV<uint64_t> fnv64;
  EXPECT_EQ(0xE4D8CB6A3646310, fnv64.hash("fnord"));
}

TEST(FNV, FNV32) {
  xzero::hash::FNV<uint32_t> fnv32;
  EXPECT_EQ(0x6D964EB0, fnv32.hash("fnord"));
}
