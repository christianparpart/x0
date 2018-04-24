#include <xzero/testing.h>
#include <xzero-flow/flowtest.h>

using namespace std;
using namespace xzero;
using namespace flow;

using Token = flowtest::Token;
using Lexer = flowtest::Lexer;
using Parser = flowtest::Parser;

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
      R"(handler main {}
      # ----
      )"};

  ASSERT_EQ(Token::InitializerMark, x.currentToken());
  ASSERT_EQ(Token::Eof, x.nextToken());
}

TEST(FlowTest, lexer_simple1) {
  flowtest::Lexer x{"input.flow",
      R"(handler main {}
      # ----
      # TokenError: bla blah
      # SyntaxError: bla yah
      )"};

  ASSERT_EQ(Token::InitializerMark, x.currentToken());
  ASSERT_EQ(Token::Begin, x.nextToken());
  ASSERT_EQ(Token::TokenError, x.nextToken());
  ASSERT_EQ(Token::Colon, x.nextToken());
}
