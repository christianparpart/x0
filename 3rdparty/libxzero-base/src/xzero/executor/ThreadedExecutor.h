// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <xzero/executor/Executor.h>
#include <deque>
#include <pthread.h>
#include <mutex>

namespace xzero {

/**
 * Executor Service using threads.
 *
 * Every executed task will be getting its own dedicated
 * system thread.
 */
class XZERO_BASE_API ThreadedExecutor : public Executor {
 public:
  ThreadedExecutor() : ThreadedExecutor(nullptr) {}
  explicit ThreadedExecutor(std::function<void(const std::exception&)> eh);
  ~ThreadedExecutor();

  void execute(const std::string& name, Task task);

  void execute(Task task) override;
  std::string toString() const override;
  void joinAll();

 private:
  static void* launchme(void* ptr);

  std::deque<pthread_t> threads_;
  std::mutex mutex_;
};

} // namespace xzero

