// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <cstdint>

namespace xzero {
namespace raft {

class Listener;

class Parser {
 public:
  explicit Parser(Listener* messageListener);

  /**
   * Parses a byte chunk into messages.
   *
   * @param chunk pointer to the byte array to be parsed.
   * @param size maximum number of bytes to process in @p chunk.
   * @param maxMessages number of messages to parse at most.
   *
   * @retval true inflight message completely parsed.
   * @retval false inflight messages not fully parsed. This means, you must
   *               not free up the passed @p chunk up to @p size yet until
   *               its last messages have been fully parsed.
   */
  bool parseFragment(const uint8_t* chunk, size_t size, int maxMessages);

  enum State {
    FRAME_HEADER_BEGIN,
    FRAME_BODY_CHUNK,
  };

 private:
  Listener* listener_;
  State state_;
};

} // namespace raft
} // namespace xzero
