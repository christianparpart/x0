// This file is part of the "libcortex" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libcortex is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/executor/Executor.h>
#include <xzero/thread/Wakeup.h>

namespace xzero {

Executor::Executor(std::unique_ptr<xzero::ExceptionHandler> eh)
    : safeCall_(std::move(eh)) {
}

Executor::~Executor() {
}

void Executor::setExceptionHandler(std::unique_ptr<ExceptionHandler> eh) {
  safeCall_.setExceptionHandler(std::move(eh));
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Executor::executeOnNextWakeup(Task  task, Wakeup* wakeup) {
  executeOnWakeup(task, wakeup, wakeup->generation());
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Executor::executeOnFirstWakeup(Task task, Wakeup* wakeup) {
  executeOnWakeup(task, wakeup, 0);
}

void Executor::safeCall(std::function<void()> callee) noexcept {
  safeCall_.invoke(callee);
}

} // namespace xzero
