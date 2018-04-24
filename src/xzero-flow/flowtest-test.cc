#include <xzero/testing.h>
#include <xzero-flow/flowtest.h>

using namespace std;
using namespace xzero;
using namespace flow;

TEST(FlowTest, lexer) {
  flowtest::Lexer t{
      "input.flow",
      R"(handler main {
      }
      # ----
      # TokenError: bla blah
      # SyntaxError: bla blah
      )"};

}
