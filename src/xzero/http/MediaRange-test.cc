// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/http/MediaRange.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

TEST(http_MediaRange, parse_quality_set) {
  Result<MediaRange> mediarange = MediaRange::parse("text/plain; q=0.2");
  ASSERT_ERROR_CODE_SUCCESS(mediarange.error());

  EXPECT_EQ("text", mediarange->type());
  EXPECT_EQ("plain", mediarange->subtype());
  EXPECT_EQ(0.2, mediarange->quality());
}

TEST(http_MediaRange, parse_quality_default) {
  Result<MediaRange> mediarange = MediaRange::parse("text/plain");
  ASSERT_ERROR_CODE_SUCCESS(mediarange.error());

  EXPECT_EQ("text", mediarange->type());
  EXPECT_EQ("plain", mediarange->subtype());
  EXPECT_EQ(1.0, mediarange->quality());
}

TEST(http_MediaRange, parse_params_ext) {
  Result<MediaRange> mediarange = MediaRange::parse(
      "text/plain; foo=bar; fnord=hort ; check = true");
  ASSERT_ERROR_CODE_SUCCESS(mediarange.error());

  const std::string* val = mediarange->getParameter("foo");
  EXPECT_TRUE(val != nullptr);
  EXPECT_EQ("bar", *val);

  val = mediarange->getParameter("fnord");
  EXPECT_TRUE(val != nullptr);
  EXPECT_EQ("hort", *val);

  val = mediarange->getParameter("check");
  EXPECT_TRUE(val != nullptr);
  EXPECT_EQ("true", *val);
}

TEST(http_MediaRange, contains_exact) {
  Result<MediaRange> mediarange = MediaRange::parse("text/plain");
  ASSERT_ERROR_CODE_SUCCESS(mediarange.error());

  EXPECT_TRUE(mediarange->contains("text/plain"));
  EXPECT_FALSE(mediarange->contains("text/csv"));
  EXPECT_FALSE(mediarange->contains("application/json"));
}

TEST(http_MediaRange, contains_subtype) {
  Result<MediaRange> mediarange = MediaRange::parse("text/*");
  ASSERT_ERROR_CODE_SUCCESS(mediarange.error());

  EXPECT_TRUE(mediarange->contains("text/plain"));
  EXPECT_TRUE(mediarange->contains("text/csv"));
  EXPECT_FALSE(mediarange->contains("application/json"));
}

TEST(http_MediaRange, parseMany) {
  // input taken from RFC 7231, section 5.3.2
  std::vector<MediaRange> accepts = *MediaRange::parseMany(
      "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c");

  ASSERT_EQ(4, accepts.size());

  EXPECT_EQ("text", accepts[0].type());
  EXPECT_EQ("plain", accepts[0].subtype());
  EXPECT_EQ(0.5, accepts[0].quality());

  EXPECT_EQ("text", accepts[1].type());
  EXPECT_EQ("html", accepts[1].subtype());
  EXPECT_EQ(1.0, accepts[1].quality());

  EXPECT_EQ("text", accepts[2].type());
  EXPECT_EQ("x-dvi", accepts[2].subtype());
  EXPECT_EQ(0.8, accepts[2].quality());

  EXPECT_EQ("text", accepts[3].type());
  EXPECT_EQ("x-c", accepts[3].subtype());
  EXPECT_EQ(1.0, accepts[3].quality());
}

TEST(http_MediaRange, match_vector) {
  // input taken from RFC 7231, section 5.3.2
  std::vector<MediaRange> accepts = *MediaRange::parseMany(
      "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c");

  auto best = MediaRange::match(accepts, {"text/plain", "text/html"});
  ASSERT_TRUE(best != nullptr);
  EXPECT_TRUE(best->contains("text/html"));

  best = MediaRange::match(accepts, {"text/plain", "application/json"});
  ASSERT_TRUE(best != nullptr);
  EXPECT_TRUE(best->contains("text/plain"));
}

TEST(http_MediaRange, match_q1_0) {
  // input taken from RFC 7231, section 5.3.2
  auto best = MediaRange::match(
      "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c",
      {"text/plain", "text/html"},
      "application/some");
  EXPECT_EQ("text/html", *best);
}

TEST(http_MediaRange, match_q0_5) {
  auto best = MediaRange::match(
      "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c",
      {"text/plain", "application/json"},
      "application/some");
  EXPECT_EQ("text/plain", *best);
}

TEST(http_MediaRange, match_q0_0) {
  auto best = MediaRange::match(
      "text/plain; q=0.5, text/html, text/x-dvi; q=0.8, text/x-c",
      {"application/json"},
      "application/some");
  EXPECT_EQ("", *best);
}
