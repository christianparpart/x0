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

TEST(StringUtilTest, TestToString) {
  EXPECT_EQ(StringUtil::toString(123), "123");
  EXPECT_EQ(StringUtil::toString(1230000000), "1230000000");
  EXPECT_EQ(StringUtil::toString(24.5), "24.5");
  EXPECT_EQ(StringUtil::toString("abc"), "abc");
  EXPECT_EQ(StringUtil::toString(std::string("abc")), "abc");
}

TEST(StringUtilTest, TestStripTrailingSlashes) {
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

TEST(StringUtilTest, TestBeginsWith) {
  EXPECT_TRUE(StringUtil::beginsWith("fnord", "fn"));
  EXPECT_TRUE(StringUtil::beginsWith("fnahrad", "fn"));
  EXPECT_FALSE(StringUtil::beginsWith("ford", "fn"));
  EXPECT_TRUE(StringUtil::beginsWith("fnord", "fnord"));
  EXPECT_FALSE(StringUtil::beginsWith("fnord", "fnordbar"));
}

TEST(StringUtilTest, beginsWithIgnoreCase) {
  EXPECT_TRUE(StringUtil::beginsWithIgnoreCase("fnord", "fN"));
  EXPECT_TRUE(StringUtil::beginsWithIgnoreCase("fnahrad", "Fn"));
  EXPECT_FALSE(StringUtil::beginsWithIgnoreCase("ford", "fN"));
  EXPECT_TRUE(StringUtil::beginsWithIgnoreCase("fnord", "fnORd"));
  EXPECT_FALSE(StringUtil::beginsWithIgnoreCase("fnord", "fnORdbaR"));
}

TEST(StringUtilTest, TestEndsWith) {
  EXPECT_TRUE(StringUtil::endsWith("fnord", "ord"));
  EXPECT_TRUE(StringUtil::endsWith("ford", "ord"));
  EXPECT_FALSE(StringUtil::endsWith("ford", "x"));
  EXPECT_FALSE(StringUtil::endsWith("ford", "fnord"));
  EXPECT_TRUE(StringUtil::endsWith("fnord", "fnord"));
  EXPECT_FALSE(StringUtil::endsWith("fnord", "fnordbar"));
}

TEST(StringUtilTest, endsWithIgnoreCase) {
  EXPECT_TRUE(StringUtil::endsWithIgnoreCase("fnord", "ORd"));
  EXPECT_TRUE(StringUtil::endsWithIgnoreCase("ford", "ORD"));
  EXPECT_FALSE(StringUtil::endsWithIgnoreCase("ford", "x"));
  EXPECT_FALSE(StringUtil::endsWithIgnoreCase("ford", "fnorD"));
  EXPECT_TRUE(StringUtil::endsWithIgnoreCase("fnord", "fnorD"));
  EXPECT_FALSE(StringUtil::endsWithIgnoreCase("fnord", "fnordbaR"));
}

TEST(StringUtilTest, TestHexPrint) {
  auto data1 = "\x17\x23\x42\x01";
  EXPECT_EQ(StringUtil::hexPrint(data1, 4), "17 23 42 01");
  EXPECT_EQ(StringUtil::hexPrint(data1, 4, false), "17234201");
  EXPECT_EQ(StringUtil::hexPrint(data1, 4, true, true), "01 42 23 17");
  EXPECT_EQ(StringUtil::hexPrint(data1, 4, false, true), "01422317");
}

TEST(StringUtilTest, TestReplaceAll) {
  std::string str =
      "cloud computing, or in simpler shorthand just >the cloud<...";

  StringUtil::replaceAll(&str, "cloud", "butt");
  EXPECT_EQ(str, "butt computing, or in simpler shorthand just >the butt<...");

  StringUtil::replaceAll(&str, "butt", "");
  EXPECT_EQ(str, " computing, or in simpler shorthand just >the <...");
}

TEST(StringUtilTest, TestFormat) {
  auto str1 = StringUtil::format(
      "The quick $0 fox jumps over the $1 dog",
      "brown",
      "lazy");

  EXPECT_EQ(str1, "The quick brown fox jumps over the lazy dog");

  auto str2 = StringUtil::format(
      "1 $3 2 $2 3 $1 4 $0 5 $8 6 $9",
      "A",
      "B",
      "C",
      "D",
      "E",
      "F",
      "G",
      "H",
      "I",
      "K");

  EXPECT_EQ(str2, "1 D 2 C 3 B 4 A 5 I 6 K");

  auto str3 = StringUtil::format("$0 + $1 = $2", 2.5, 6.5, 9);
  EXPECT_EQ(str3, "2.5 + 6.5 = 9");

  auto str5 = StringUtil::format("$0, $1", 1.0, 0.0625);
  EXPECT_EQ(str5, "1.0, 0.0625");

  auto str4 = StringUtil::format("$1$1$1$1$1 $0", "Batman", "Na");
  EXPECT_EQ(str4, "NaNaNaNaNa Batman");
}

TEST(StringUtilTest, splitByAny_empty) {
  auto parts = StringUtil::splitByAny("", ", \t");
  EXPECT_EQ(0, parts.size());

  parts = StringUtil::splitByAny(", \t", ", \t");
  EXPECT_EQ(0, parts.size());
}

TEST(StringUtilTest, splitByAny) {
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

TEST(StringUtilTest, TestSplit) {
  auto parts1 = StringUtil::split("one,two,three", ",");
  EXPECT_EQ(parts1.size(), 3);
  EXPECT_EQ(parts1[0], "one");
  EXPECT_EQ(parts1[1], "two");
  EXPECT_EQ(parts1[2], "three");

  auto parts2 = StringUtil::split("onexxtwoxxthree", "xx");
  EXPECT_EQ(parts2.size(), 3);
  EXPECT_EQ(parts2[0], "one");
  EXPECT_EQ(parts2[1], "two");
  EXPECT_EQ(parts2[2], "three");
}
