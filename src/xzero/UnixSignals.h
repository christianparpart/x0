// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero/executor/Executor.h>
#include <xzero/UnixSignalInfo.h>
#include <memory>

namespace xzero {

struct UnixSignalInfo;

/**
 * UNIX Signal Notification API.
 */
class UnixSignals {
 public:
  typedef std::function<void(const UnixSignalInfo&)> SignalHandler;
  typedef Executor::HandleRef HandleRef;

  virtual ~UnixSignals() {}

  /**
   * Runs given @p task when given @p signal was received.
   *
   * @param signo UNIX signal number (such as @c SIGTERM) you want
   *              to be called for.
   * @param task  The SignalHandler to execute upon given event.
   *
   * @note If you always want to get notified on a given signal, you must
   *       reregister yourself each time you have been fired.
   */
  virtual HandleRef notify(int signo, SignalHandler task) = 0;

  /**
   * Creates a platform-dependant instance of UnixSignals.
   */
  static std::unique_ptr<UnixSignals> create(Executor* executor);

  /**
   * Blocks given signal.
   *
   * You can still handle this signal via the UnixSignals API,
   * but not with native system API.
   */
  static void blockSignal(int signo);

  /**
   * Unblocks given signal.
   */
  static void unblockSignal(int signo);

  /**
   * Retrieves the (technical) string representation of the given signal number.
   *
   * @param signo the signal number to convert into a string.
   *
   * @return the 1:1 string representation of the signal number, such as
   *         the string @c "SIGTERM" for the signal @c SIGTERM.
   */
  static std::string toString(int signo);

 protected:
  class SignalWatcher;
};

class UnixSignals::SignalWatcher : public Executor::Handle {
 public:
  typedef Executor::Task Task;

  explicit SignalWatcher(UnixSignals::SignalHandler action)
      : action_(action) {};

  void fire() {
    Executor::Handle::fire(std::bind(action_, info));
  }

  UnixSignalInfo info;

 private:
  UnixSignals::SignalHandler action_;
};

} // namespace xzero
