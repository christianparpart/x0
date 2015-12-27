// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/http/hpack/StaticTable.h>
#include <xzero/Application.h>
#include <gtest/gtest.h>

using xzero::http::hpack::DynamicTable;
using xzero::http::hpack::StaticTable;
using namespace xzero;

TEST(http_hpack_DynamicTable, walkthrough) {
  Application::logToStderr(LogLevel::Trace);

  DynamicTable dt(45);

  dt.add({"Hello", "World"}); // adds 42
  EXPECT_EQ(42, dt.size());

  dt.add({"Bongo", "Yolo"});  // adds 41, removing the first
  EXPECT_EQ(41, dt.size());
  ASSERT_EQ(1, dt.length());
}

TEST(http_hpack_DynamicTable, evict_to_zero) {
  DynamicTable dt(45);
  dt.add({"Hello", "World"}); // adds 42

  dt.setMaxSize(40);
  EXPECT_EQ(0, dt.size());

  dt.add({"Bongo", "Yolo"});  // would add 41; but not added at all
  EXPECT_EQ(0, dt.size());
}

TEST(http_hpack_DynamicTable, find) {
  DynamicTable dt(16384);
  dt.add(StaticTable::at(2)); // {:method, GET}
  dt.add(StaticTable::at(4)); // {:path, /}

  bool fullMatch;
  size_t index = dt.find(StaticTable::at(2), &fullMatch);
  ASSERT_EQ(1, index);
  ASSERT_TRUE(fullMatch);

  index = dt.find(StaticTable::at(7), &fullMatch);
  ASSERT_EQ((size_t) DynamicTable::npos, (size_t) index);
}

