// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <flow/util/PrefixTree.h>

TEST(PrefixTree, exactMatch) {
  xzero::flow::util::PrefixTree<std::string, int> t;
  t.insert("/foo", 1);
  t.insert("/foo/bar", 2);
  t.insert("/foo/fnord", 3);

  int out{0};
  EXPECT_TRUE(t.lookup("/foo", &out));
  EXPECT_EQ(1, out);

  EXPECT_TRUE(t.lookup("/foo/bar", &out));
  EXPECT_EQ(2, out);

  EXPECT_TRUE(t.lookup("/foo/fnord", &out));
  EXPECT_EQ(3, out);
}

TEST(PrefixTree, subMatch) {
  xzero::flow::util::PrefixTree<std::string, int> t;
  t.insert("/foo", 1);
  t.insert("/foo/bar", 2);
  t.insert("/foo/fnord", 3);

  int out{0};
  EXPECT_TRUE(t.lookup("/foo/index.html", &out));
  EXPECT_EQ(1, out);

  EXPECT_TRUE(t.lookup("/foo/bar/", &out));
  EXPECT_EQ(2, out);

  EXPECT_TRUE(t.lookup("/foo/fnord/HACKING.md", &out));
  EXPECT_EQ(3, out);
}

