// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//
#pragma once

#include <xzero/executor/Scheduler.h>

namespace xzero {

/**
 * Interface for scheduling tasks on the local thread.
 */
class LocalScheduler : public Scheduler {
 public:
  LocalScheduler(std::unique_ptr<ExceptionHandler> eh);

  std::string toString() const override;
  void execute(Task task) override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime ts, Task task) override;
  HandleRef executeOnReadable(int fd, Task task,
                              Duration timeout, Task onTimeout) override;
  HandleRef executeOnWritable(int fd, Task task,
                              Duration timeout, Task onTimeout) override;
  void cancelFD(int fd) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;
  size_t timerCount() override;
  size_t readerCount() override;
  size_t writerCount() override;
  size_t taskCount() override;
  void runLoop() override;
  void runLoopOnce() override;
  void breakLoop() override;
};

}  // namespace xzero
