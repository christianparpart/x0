// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <flow/util/SuffixTree.h>

TEST(SuffixTree, exactMatch) {
  xzero::flow::util::SuffixTree<std::string, int> t;
  t.insert("www.example.com.", 1);
  t.insert("example.com.", 2);
  t.insert("com.", 3);

  int out{0};
  EXPECT_TRUE(t.lookup("www.example.com.", &out));
  EXPECT_EQ(1, out);

  EXPECT_TRUE(t.lookup("example.com.", &out));
  EXPECT_EQ(2, out);

  EXPECT_TRUE(t.lookup("com.", &out));
  EXPECT_EQ(3, out);
}

TEST(SuffixTree, subMatch) {
  xzero::flow::util::SuffixTree<std::string, int> t;
  t.insert("www.example.com.", 1);
  t.insert(    "example.com.", 2);
  t.insert(            "com.", 3);

  int out{0};
  EXPECT_TRUE(t.lookup("mirror.www.example.com.", &out));
  EXPECT_EQ(1, out);

  EXPECT_TRUE(t.lookup("www2.example.com.", &out));
  EXPECT_EQ(2, out);

  EXPECT_TRUE(t.lookup("foo.com.", &out));
  EXPECT_EQ(3, out);
}
