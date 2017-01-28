// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/http/hpack/StaticTable.h>
#include <xzero/testing.h>

using xzero::http::hpack::DynamicTable;
using xzero::http::hpack::StaticTable;
using namespace xzero;

TEST(hpack_DynamicTable, walkthrough) {
  DynamicTable dt(45);

  dt.add("Hello", "World"); // adds 42
  EXPECT_EQ(42, dt.size());

  dt.add("Bongo", "Yolo");  // adds 41, removing the first
  EXPECT_EQ(41, dt.size());
  ASSERT_EQ(1, dt.length());
}

TEST(hpack_DynamicTable, evict_to_zero) {
  DynamicTable dt(45);
  dt.add("Hello", "World"); // adds 42

  dt.setMaxSize(40);
  EXPECT_EQ(0, dt.size());

  dt.add("Bongo", "Yolo");  // would add 41; but not added at all
  EXPECT_EQ(0, dt.size());
}

TEST(hpack_DynamicTable, find) {
  DynamicTable dt(16384);
  dt.add(StaticTable::at(2)); // {:method, GET}
  dt.add(StaticTable::at(4)); // {:path, /}

  size_t index;
  bool fullMatch;

  bool match = dt.find(StaticTable::at(2), &index, &fullMatch);

  ASSERT_TRUE(match);
  ASSERT_EQ(1, index);
  ASSERT_TRUE(fullMatch);

  match = dt.find(StaticTable::at(7), &index, &fullMatch);
  ASSERT_FALSE(match);
}

