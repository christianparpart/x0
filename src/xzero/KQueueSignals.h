// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/UnixSignals.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/RefPtr.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <list>
#include <signal.h>

namespace xzero {

class Executor;

class KQueueSignals : public UnixSignals {
 public:
  explicit KQueueSignals(Executor* executor);
  ~KQueueSignals();
  HandleRef notify(int signo, SignalHandler task) override;
  void onSignal();

 private:
  Executor* executor_;
  FileDescriptor fd_;
  sigset_t oldSignalMask_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
  std::atomic<size_t> interests_;
  std::mutex mutex_;
};

} // namespace xzero
