#include <xzero/testing.h>
#include <xzero/text/literals.h>
#include <flow/flowtest.h>

#include "flowtest.cc"

using namespace std;
using namespace xzero;
using namespace flow;

using flowtest::Token;
using flowtest::DiagnosticsType;

// ---------------------------------------------------------------------------
// Lexer

TEST(FlowTest, lexer_empty1) {
  flowtest::Lexer x{"input.flow", ""};
  ASSERT_EQ(Token::Eof, x.currentToken());
}

TEST(FlowTest, lexer_empty2) {
  flowtest::Lexer x{"input.flow", "handler main {}"};
  ASSERT_EQ(Token::Eof, x.currentToken());
}

TEST(FlowTest, lexer_empty3) {
  flowtest::Lexer x{"input.flow",
      R"(|handler main {}
         |# ----
         |)"_multiline};

  ASSERT_EQ(Token::InitializerMark, x.currentToken());
  ASSERT_EQ(Token::Eof, x.nextToken());
}

TEST(FlowTest, lexer_simple1) {
  flowtest::Lexer x{"input.flow",
      R"(|handler main {}
         |# ----
         |# TokenError: bla blah
         )"_multiline};

  ASSERT_EQ(Token::InitializerMark, x.currentToken());

  ASSERT_EQ(Token::Begin, x.nextToken());
  ASSERT_EQ(Token::TokenError, x.nextToken());
  ASSERT_EQ(Token::Colon, x.nextToken());
  ASSERT_EQ(Token::MessageText, x.nextToken());
  ASSERT_EQ("bla blah", x.stringValue());
  ASSERT_EQ(Token::LF, x.nextToken());

  ASSERT_EQ(Token::Eof, x.nextToken());
}

TEST(FlowTest, lexer_simple2) {
  flowtest::Lexer x{"input.flow",
      R"(|handler main {}
         |# ----
         |# TokenError: bla blah
         |# SyntaxError: bla yah
         )"_multiline};

  ASSERT_EQ(Token::InitializerMark, x.currentToken());

  ASSERT_EQ(Token::Begin, x.nextToken());
  ASSERT_EQ(Token::TokenError, x.nextToken());
  ASSERT_EQ(Token::Colon, x.nextToken());
  ASSERT_EQ(Token::MessageText, x.nextToken());
  ASSERT_EQ("bla blah", x.stringValue());
  ASSERT_EQ(Token::LF, x.nextToken());

  ASSERT_EQ(Token::Begin, x.nextToken());
  ASSERT_EQ(Token::SyntaxError, x.nextToken());
  ASSERT_EQ(Token::Colon, x.nextToken());
  ASSERT_EQ(Token::MessageText, x.nextToken());
  ASSERT_EQ("bla yah", x.stringValue());
  ASSERT_EQ(Token::LF, x.nextToken());

  ASSERT_EQ(Token::Eof, x.nextToken());
}

// ---------------------------------------------------------------------------
// Parser

TEST(FlowTest, parser_simple1) {
  flowtest::Parser p{"input.flow",
      R"(|handler main {}
         |# ----
         |# TokenError: bla blah
         )"_multiline};

  flow::diagnostics::Report report;
  p.parse(&report);
  ASSERT_EQ(1, report.size());
  ASSERT_EQ(DiagnosticsType::TokenError, report[0].type);
  ASSERT_EQ("bla blah", report[0].text);
}

TEST(FlowTest, parser_simple2) {
  flowtest::Parser p{"input.flow",
      R"(|handler main {}
         |# ----
         |# TokenError: bla blah
         |# SyntaxError: bla yah
         )"_multiline};

  flow::diagnostics::Report report;
  p.parse(&report);
  ASSERT_EQ(2, report.size());

  ASSERT_EQ(DiagnosticsType::TokenError, report[0].type);
  ASSERT_EQ("bla blah", report[0].text);

  ASSERT_EQ(DiagnosticsType::TokenError, report[0].type);
  ASSERT_EQ("bla yah", report[1].text);
}
