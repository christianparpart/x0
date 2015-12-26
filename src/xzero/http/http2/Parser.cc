#include <xzero/http/http2/Parser.h>
#include <xzero/http/http2/FrameListener.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {
namespace http2 {

Parser::Parser(FrameListener* listener)
    : listener_(listener),
      state_(ParserState::Idle) {
}

bool Parser::parseFragment(const BufferRef& chunk) {
  return false; // TODO
}

} // namespace http2
} // namespace http
} // namespace xzero
