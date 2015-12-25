#include <gtest/gtest.h>
#include <xzero/http/http2/framing.h>

using namespace xzero;
using namespace xzero::http::http2;

TEST(http_http2_Frame, size) {
  ASSERT_EQ(9, sizeof(Frame));
}

TEST(http_http2_Frame, encodeHeader) {
  // TODO
}
