// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <stdlib.h>
#include <stdio.h>
#include <xzero/StringUtil.h>
#include <xzero/testing.h>

using namespace xzero;

TEST(StringUtil, TestStripTrailingSlashes) {
  std::string s1 = "fnord/bar/";
  StringUtil::stripTrailingSlashes(&s1);
  EXPECT_EQ(s1, "fnord/bar");

  std::string s2 = "fnord/bar///";
  StringUtil::stripTrailingSlashes(&s2);
  EXPECT_EQ(s2, "fnord/bar");

  std::string s3 = "fnord/bar";
  StringUtil::stripTrailingSlashes(&s3);
  EXPECT_EQ(s3, "fnord/bar");

  std::string s4 = "/";
  StringUtil::stripTrailingSlashes(&s4);
  EXPECT_EQ(s4, "");
}

TEST(StringUtil, TestBeginsWith) {
  EXPECT_TRUE(StringUtil::beginsWith("fnord", "fn"));
  EXPECT_TRUE(StringUtil::beginsWith("fnahrad", "fn"));
  EXPECT_FALSE(StringUtil::beginsWith("ford", "fn"));
  EXPECT_TRUE(StringUtil::beginsWith("fnord", "fnord"));
  EXPECT_FALSE(StringUtil::beginsWith("fnord", "fnordbar"));
}

TEST(StringUtil, beginsWithIgnoreCase) {
  EXPECT_TRUE(StringUtil::beginsWithIgnoreCase("fnord", "fN"));
  EXPECT_TRUE(StringUtil::beginsWithIgnoreCase("fnahrad", "Fn"));
  EXPECT_FALSE(StringUtil::beginsWithIgnoreCase("ford", "fN"));
  EXPECT_TRUE(StringUtil::beginsWithIgnoreCase("fnord", "fnORd"));
  EXPECT_FALSE(StringUtil::beginsWithIgnoreCase("fnord", "fnORdbaR"));
}

TEST(StringUtil, TestEndsWith) {
  EXPECT_TRUE(StringUtil::endsWith("fnord", "ord"));
  EXPECT_TRUE(StringUtil::endsWith("ford", "ord"));
  EXPECT_FALSE(StringUtil::endsWith("ford", "x"));
  EXPECT_FALSE(StringUtil::endsWith("ford", "fnord"));
  EXPECT_TRUE(StringUtil::endsWith("fnord", "fnord"));
  EXPECT_FALSE(StringUtil::endsWith("fnord", "fnordbar"));
}

TEST(StringUtil, endsWithIgnoreCase) {
  EXPECT_TRUE(StringUtil::endsWithIgnoreCase("fnord", "ORd"));
  EXPECT_TRUE(StringUtil::endsWithIgnoreCase("ford", "ORD"));
  EXPECT_FALSE(StringUtil::endsWithIgnoreCase("ford", "x"));
  EXPECT_FALSE(StringUtil::endsWithIgnoreCase("ford", "fnorD"));
  EXPECT_TRUE(StringUtil::endsWithIgnoreCase("fnord", "fnorD"));
  EXPECT_FALSE(StringUtil::endsWithIgnoreCase("fnord", "fnordbaR"));
}

TEST(StringUtil, TestHexPrint) {
  auto data1 = "\x17\x23\x42\x01";
  EXPECT_EQ(StringUtil::hexPrint(data1, 4), "17 23 42 01");
  EXPECT_EQ(StringUtil::hexPrint(data1, 4, false), "17234201");
  EXPECT_EQ(StringUtil::hexPrint(data1, 4, true, true), "01 42 23 17");
  EXPECT_EQ(StringUtil::hexPrint(data1, 4, false, true), "01422317");
}

TEST(StringUtil, TestReplaceAll) {
  std::string str =
      "cloud computing, or in simpler shorthand just >the cloud<...";

  StringUtil::replaceAll(&str, "cloud", "butt");
  EXPECT_EQ(str, "butt computing, or in simpler shorthand just >the butt<...");

  StringUtil::replaceAll(&str, "butt", "");
  EXPECT_EQ(str, " computing, or in simpler shorthand just >the <...");
}

TEST(StringUtil, splitByAny_empty) {
  auto parts = StringUtil::splitByAny("", ", \t");
  EXPECT_EQ(0, parts.size());

  parts = StringUtil::splitByAny(", \t", ", \t");
  EXPECT_EQ(0, parts.size());
}

TEST(StringUtil, splitByAny) {
  auto parts = StringUtil::splitByAny("\tone, two , three four\t", ", \t");

  int i = 0;
  for (const auto& part: parts) {
    logf("part[$0]: '$1'", i, part);
    i++;
  }

  ASSERT_EQ(4, parts.size());
  EXPECT_EQ("one", parts[0]);
  EXPECT_EQ("two", parts[1]);
  EXPECT_EQ("three", parts[2]);
  EXPECT_EQ("four", parts[3]);
}

TEST(StringUtil, TestSplit_empty) {
  auto parts = StringUtil::split("", ",");

  EXPECT_EQ(0, parts.size());
}

TEST(StringUtil, TestSplit) {
  auto parts1 = StringUtil::split("one,two,three", ",");
  EXPECT_EQ(3, parts1.size());
  EXPECT_EQ("one", parts1[0]);
  EXPECT_EQ("two", parts1[1]);
  EXPECT_EQ("three", parts1[2]);

  auto parts2 = StringUtil::split("onexxtwoxxthree", "xx");
  EXPECT_EQ(3, parts2.size());
  EXPECT_EQ("one", parts2[0]);
  EXPECT_EQ("two", parts2[1]);
  EXPECT_EQ("three", parts2[2]);
}
