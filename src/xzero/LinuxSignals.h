// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/UnixSignals.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/RefPtr.h>

#include <vector>
#include <list>
#include <atomic>
#include <mutex>
#include <signal.h>

namespace xzero {

/**
 * LinuxSignals implements UNIX signal handling using Linux optimized
 * and dependant operationg system features.
 *
 * @see PosixSignals
 */
class LinuxSignals : public UnixSignals {
 public:
  explicit LinuxSignals(Executor* executor);
  ~LinuxSignals();

  HandleRef notify(int signo, SignalHandler task) override;

 private:
  void onSignal();

 private:
  Executor* executor_;
  FileDescriptor fd_;
  sigset_t signalMask_;
  std::mutex mutex_;
  std::atomic<size_t> interests_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
};

} // namespace xzero
