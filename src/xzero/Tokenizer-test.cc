// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/Tokenizer.h>
#include <xzero/Buffer.h>

class TokenizerTest : public ::xzero::testing::Test {
 public:
  typedef xzero::Tokenizer<xzero::BufferRef, xzero::BufferRef> BufferTokenizer;

  void SetUp();
  void TearDown();

  void Tokenize3();
};

void TokenizerTest::SetUp() {}

void TokenizerTest::TearDown() {}

TEST_F(TokenizerTest, Tokenize3) {
  xzero::Buffer input("/foo/bar/com");
  BufferTokenizer st(input.ref(1), "/");
  std::vector<xzero::BufferRef> tokens = st.tokenize();

  ASSERT_EQ(3, tokens.size());
  EXPECT_EQ("foo", tokens[0]);
  EXPECT_EQ("bar", tokens[1]);
  EXPECT_EQ("com", tokens[2]);
}
