#include <x0d/XzeroEventHandler.h>
#include <x0d/XzeroDaemon.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>

namespace x0d {

using namespace xzero;

XzeroEventHandler::XzeroEventHandler(XzeroDaemon* daemon,
                                     xzero::Executor* executor)
    : daemon_(daemon),
      executor_(executor),
      state_(XzeroState::Inactive) {

  executor_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onConfigReload, this));
  executor_->executeOnSignal(SIGUSR1, std::bind(&XzeroEventHandler::onCycleLogs, this));
  executor_->executeOnSignal(SIGUSR2, std::bind(&XzeroEventHandler::onUpgradeBinary, this));
  executor_->executeOnSignal(SIGQUIT, std::bind(&XzeroEventHandler::onGracefulShutdown, this));
  executor_->executeOnSignal(SIGTERM, std::bind(&XzeroEventHandler::onFastShutdown, this));
  executor_->executeOnSignal(SIGINT, std::bind(&XzeroEventHandler::onFastShutdown, this));
}

XzeroEventHandler::~XzeroEventHandler() {
}

void XzeroEventHandler::onConfigReload() {
  /* TODO
   * 1. suspend the world
   * 2. load new config file
   * 3. run setup handler with producing a diff to what is to be removed
   * 4. Undo anything that's not in setup handler anymore (e.g. tcp listeners)
   * 5. replace main request handler with new one
   * 6. run post-config
   * 6. resume the world
   */

  executor_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onConfigReload, this));
}

void XzeroEventHandler::onCycleLogs() {
  logNotice("x0d", "Reload signal received.");

  daemon_->onCycleLogs();

  executor_->executeOnSignal(SIGUSR1, std::bind(&XzeroEventHandler::onCycleLogs, this));
}

void XzeroEventHandler::onUpgradeBinary() {
  /* TODO
   * 1. suspend the world
   * 2. save state into temporary file with an inheriting file descriptor
   * 3. exec into new binary
   * 4. (new process) load state from file descriptor and close fd
   * 5. (new process) resume the world
   */
}

void XzeroEventHandler::onFastShutdown() {
  logNotice("x0d", "Initiating fast shutdown.");
  daemon_->terminate();
}

void XzeroEventHandler::onGracefulShutdown() {
  logNotice("x0d", "Initiating graceful shutdown.");

  /* TODO
   * 1. stop all listeners
   * 2. wait until all requests have been handled.
   * 3. orderly shutdown
   */
}

} // namespace x0d
