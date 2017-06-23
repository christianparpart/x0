// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/UnixSignals.h>
#include <xzero/RefPtr.h>
#include <vector>
#include <list>
#include <mutex>
#include <signal.h>

namespace xzero {

/**
 * PosixSignals implements UNIX signal handling using standard POSIX API.
 */
class PosixSignals : public UnixSignals {
 public:
  explicit PosixSignals(Executor* executor);
  ~PosixSignals();

  HandleRef notify(int signo, SignalHandler task) override;

  static PosixSignals* get();

 private:
  static PosixSignals* singleton_;
  static void onSignal(int signo, siginfo_t* info, void* p);
  void onSignal2(int signo, siginfo_t* info, void* p);

 private:
  Executor* executor_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
  std::mutex mutex_;
};

} // namespace xzero
