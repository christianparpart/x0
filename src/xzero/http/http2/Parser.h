// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/http2/StreamID.h>
#include <string>

namespace xzero {

class BufferRef;

namespace http {
namespace http2 {

class FrameListener;

enum class ParserState {
  Idle,
  Data,
  Headers,
  Priority,
  Reset,
};

class Parser {
 public:
  explicit Parser(FrameListener* listener);

  size_t parseFragment(const BufferRef& chunk);
  void parseFrame(const BufferRef& frame);

 protected:
  void data();
  void headers(uint8_t flags, StreamID sid, const BufferRef& payload);
  void priority();
  void resetStream();

 private:
  FrameListener* listener_;
  ParserState state_;
};

} // namespace http2
} // namespace http
} // namespace xzero
