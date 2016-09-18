// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <vector>

namespace xzero {
namespace raft {

/**
 * Abstracts the systems state machine.
 */
class StateMachine {
 public:
  virtual ~StateMachine() {}

  virtual void loadSnapshotBegin() = 0;
  virtual void loadSnapshotChunk(const std::vector<uint8_t>& chunk) = 0;
  virtual void loadSnapshotEnd() = 0;

  /**
   * Applies given @p command to this state machine.
   *
   * It is assured that the command is comitted (persisted on the majority
   * of cluster members).
   */
  virtual void applyCommand(const Command& command) = 0;
};

} // namespace raft
} // namespace xzero
