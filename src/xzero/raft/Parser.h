// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/util/BinaryReader.h>
#include <xzero/Buffer.h>
#include <cstdint>

namespace xzero {
namespace raft {

class Listener;

/**
 * Parses a stream of Raft messages.
 */
class Parser {
 public:
  Parser(Id fromId, Listener* messageListener);

  /**
   * Parses a byte chunk into messages.
   *
   * Fills the internal buffer. As soon as it contains at least one message
   * it'll start parsing that message and calling the respective
   * callbacks in the message @c Listener.
   *
   * @param chunk pointer to the byte array to be parsed.
   * @param size maximum number of bytes to process in @p chunk.
   *
   * @returns number of fully parsed messages.
   */
  unsigned parseFragment(const uint8_t* chunk, size_t size);

  BufferRef availableBytes() const;

 private:
  bool parseFrame();
  void parseVoteRequest();
  void parseVoteResponse();
  void parseAppendEntriesRequest();
  void parseAppendEntriesResponse();
  void parseInstallSnapshotRequest();
  void parseInstallSnapshotResponse();

 private:
  Buffer inputBuffer_;
  size_t inputOffset_;
  BinaryReader reader_;

  Id fromId_;
  Listener* listener_;
};

inline BufferRef Parser::availableBytes() const {
  return inputBuffer_.ref(inputOffset_);
}

} // namespace raft
} // namespace xzero
