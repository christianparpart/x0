// This file is part of the "libcortex" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libcortex is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/sysconfig.h>
#include <xzero/executor/Executor.h>
#include <deque>

namespace xzero {

/**
 * Executor to directly invoke the tasks being passed by the caller.
 *
 * @note Not thread-safe.
 */
class LocalExecutor : public Executor {
 public:
  LocalExecutor(
    bool recursive = false,
    std::unique_ptr<xzero::ExceptionHandler> eh = nullptr);

  std::string toString() const override;
  void execute(Task task) override;
  HandleRef executeOnReadable(int fd, Task task, Duration timeout, Task onTimeout) override;
  HandleRef executeOnWritable(int fd, Task task, Duration timeout, Task onTimeout) override;
  void cancelFD(int fd) override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime ts, Task task) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;

  /** Tests whether this executor is currently running some task. */
  bool isRunning() const { return running_ > 0; }

  /** Tests whether this executor allows recursive execution of tasks. */
  bool isRecursive() const { return recursive_; }

  /** Sets wether recursive execution is allowed or flattened. */
  void setRecursive(bool value) { recursive_ = value; }

  /** Retrieves number of deferred tasks. */
  size_t backlog() const { return deferred_.size(); }

 private:
  bool recursive_;
  int running_;
  std::deque<Task> deferred_;
};

} // namespace xzero
