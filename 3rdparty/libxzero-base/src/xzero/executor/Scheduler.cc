// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/executor/Scheduler.h>
#include <xzero/thread/Wakeup.h>

namespace xzero {

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Scheduler::executeOnNextWakeup(Task  task, Wakeup* wakeup) {
  executeOnWakeup(task, wakeup, wakeup->generation());
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Scheduler::executeOnFirstWakeup(Task task, Wakeup* wakeup) {
  executeOnWakeup(task, wakeup, 0);
}

}  // namespace xzero
