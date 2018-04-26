#include <xzero/testing.h>
#include <xzero/text/literals.h>
#include <xzero-flow/flowtest.h>

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

  Result<flowtest::ParseResult> pr = p.parse();
  ASSERT_TRUE(pr);
  ASSERT_EQ("handler main {}\n", pr->program);
  ASSERT_EQ(1, pr->messages.size());
  ASSERT_EQ(DiagnosticsType::TokenError, pr->messages[0].type);
  ASSERT_EQ(1, pr->messages[0].texts.size());
  ASSERT_EQ("bla blah", pr->messages[0].texts[0]);
}

TEST(FlowTest, parser_simple2) {
  flowtest::Parser p{"input.flow",
      R"(|handler main {}
         |# ----
         |# TokenError: bla blah
         |# SyntaxError: bla yah
         )"_multiline};

  Result<flowtest::ParseResult> pr = p.parse();
  ASSERT_TRUE(pr);
  ASSERT_EQ("handler main {}\n", pr->program);
  ASSERT_EQ(2, pr->messages.size());

  ASSERT_EQ(DiagnosticsType::TokenError, pr->messages[0].type);
  ASSERT_EQ(1, pr->messages[0].texts.size());
  ASSERT_EQ("bla blah", pr->messages[0].texts[0]);

  ASSERT_EQ(DiagnosticsType::TokenError, pr->messages[0].type);
  ASSERT_EQ(1, pr->messages[1].texts.size());
  ASSERT_EQ("bla yah", pr->messages[1].texts[0]);
}
