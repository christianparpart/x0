// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/io/InputStream.h>
#include <xzero/io/OutputStream.h>
#include <system_error>
#include <memory>

namespace xzero {
namespace raft {

/**
 * Abstracts the systems state machine.
 */
class StateMachine {
 public:
  virtual ~StateMachine() {}

  /**
   * Loads a snapshot of a full FSM state into this instance.
   */
  virtual std::error_code loadSnapshot(std::unique_ptr<InputStream>&& input) = 0;

  /**
   * Retrieves a full snapshot of this FSM.
   */
  virtual std::error_code saveSnapshot(std::unique_ptr<OutputStream>&& output) = 0;

  /**
   * Applies given @p command to this state machine.
   *
   * It is assured that the command is comitted (persisted on the majority
   * of cluster members).
   */
  virtual Reply applyCommand(const Command& command) = 0;
};

} // namespace raft
} // namespace xzero
