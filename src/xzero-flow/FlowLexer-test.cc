// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/FlowLexer.h>
#include <xzero-flow/FlowToken.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::flow;

TEST(FlowLexer, eof) {
  FlowLexer lexer;
  lexer.openString("");

  ASSERT_EQ(FlowToken::Eof, lexer.token());
  ASSERT_EQ(FlowToken::Eof, lexer.nextToken());
  ASSERT_EQ(FlowToken::Eof, lexer.token());
  ASSERT_EQ(1, lexer.line());
  ASSERT_EQ(1, lexer.column());
}

TEST(FlowLexer, token_keywords) {
  FlowLexer lexer;
  lexer.openString("handler");

  ASSERT_EQ(FlowToken::Handler, lexer.token());
  ASSERT_EQ(1, lexer.line());
  ASSERT_EQ(7, lexer.column());
}

TEST(FlowLexer, composed) {
  FlowLexer lexer;
  lexer.openString("handler main {}");

  ASSERT_EQ(FlowToken::Handler, lexer.token());
  ASSERT_EQ("handler", lexer.stringValue());

  ASSERT_EQ(FlowToken::Ident, lexer.nextToken());
  ASSERT_EQ("main", lexer.stringValue());

  ASSERT_EQ(FlowToken::Begin, lexer.nextToken());

  ASSERT_EQ(FlowToken::End, lexer.nextToken());

  ASSERT_EQ(FlowToken::Eof, lexer.nextToken());

  ASSERT_EQ(FlowToken::Eof, lexer.nextToken());
}
