// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/UnixSignals.h>
#include <xzero/defines.h>
#include <memory>
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

#if defined(XZERO_OS_WIN32)
  static void onSignal(int signo);
#else
  static void onSignal(int signo, siginfo_t* info, void* p);
#endif

  void onSignal2(int signo, int pid, int uid, void* p);

 private:
  Executor* executor_;
  std::vector<std::list<std::shared_ptr<SignalWatcher>>> watchers_;
  std::mutex mutex_;
};

} // namespace xzero
