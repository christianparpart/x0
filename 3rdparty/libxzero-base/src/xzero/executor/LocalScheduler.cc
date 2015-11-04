// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/executor/LocalScheduler.h>
#include <xzero/logging.h>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("LocalScheduler", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

using HandleRef = LocalScheduler::HandleRef;

LocalScheduler::LocalScheduler(std::unique_ptr<ExceptionHandler> eh)
    : Scheduler(std::move(eh)) {
}

std::string LocalScheduler::toString() const {
  return "LocalScheduler";
}

void LocalScheduler::execute(Task task) {
  TRACE("execute task", ";)");
  safeCall(task);
}

HandleRef LocalScheduler::executeAfter(Duration delay, Task task) {
  // TODO
  return HandleRef(new Handle());
}

HandleRef LocalScheduler::executeAt(UnixTime ts, Task task) {
  // TODO
  return HandleRef(new Handle());
}

HandleRef LocalScheduler::executeOnReadable(int fd, Task task,
                                            Duration timeout, Task onTimeout) {
  // TODO
  TRACE("executeOnReadable() fd=$0", fd);
  safeCall(task);
  return HandleRef(new Handle());
}

HandleRef LocalScheduler::executeOnWritable(int fd, Task task,
                                            Duration timeout, Task onTimeout) {
  // TODO
  TRACE("executeOnWritable() fd=$0", fd);
  safeCall(task);
  return HandleRef(new Handle());
}

void LocalScheduler::cancelFD(int fd) {
}

void LocalScheduler::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  // TODO
}

size_t LocalScheduler::timerCount() {
  return 0;
}

size_t LocalScheduler::readerCount() {
  return 0;
}

size_t LocalScheduler::writerCount() {
  return 0;
}

size_t LocalScheduler::taskCount() {
  return 0;
}

void LocalScheduler::runLoop() {
}

void LocalScheduler::runLoopOnce() {
}

void LocalScheduler::breakLoop() {
}

}  // namespace xzero
