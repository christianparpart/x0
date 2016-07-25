// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroEventHandler.h>
#include <x0d/XzeroDaemon.h>
#include <xzero/UnixSignalInfo.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <signal.h>

namespace x0d {

using namespace xzero;

XzeroEventHandler::XzeroEventHandler(XzeroDaemon* daemon,
                                     xzero::Executor* executor)
    : daemon_(daemon),
      executor_(executor),
      state_(XzeroState::Inactive) {

  executor_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onConfigReload, this));
  executor_->executeOnSignal(SIGUSR1, std::bind(&XzeroEventHandler::onCycleLogs, this, std::placeholders::_1));
  executor_->executeOnSignal(SIGUSR2, std::bind(&XzeroEventHandler::onUpgradeBinary, this, std::placeholders::_1));
  executor_->executeOnSignal(SIGQUIT, std::bind(&XzeroEventHandler::onGracefulShutdown, this));
  executor_->executeOnSignal(SIGTERM, std::bind(&XzeroEventHandler::onQuickShutdown, this));
  executor_->executeOnSignal(SIGINT, std::bind(&XzeroEventHandler::onQuickShutdown, this));
}

XzeroEventHandler::~XzeroEventHandler() {
}

void XzeroEventHandler::onConfigReload() {
  logNotice("x0d", "Reloading configuration.");
  daemon_->reloadConfiguration();

  executor_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onConfigReload, this));
}

void XzeroEventHandler::onCycleLogs(const xzero::UnixSignalInfo& info) {
  logNotice("x0d", "Reload signal received.");

  daemon_->onCycleLogs();

  executor_->executeOnSignal(SIGUSR1, std::bind(&XzeroEventHandler::onCycleLogs, this, std::placeholders::_1));
}

void XzeroEventHandler::onUpgradeBinary(const UnixSignalInfo& info) {
  logNotice("x0d",
            "Upgrading binary requested by pid $0 uid $1",
            info.pid.get(), info.uid.get());

  /* TODO [x0d] binary upgrade
   * 1. suspend the world
   * 2. save state into temporary file with an inheriting file descriptor
   * 3. exec into new binary
   * 4. (new process) load state from file descriptor and close fd
   * 5. (new process) resume the world
   */
}

void XzeroEventHandler::onQuickShutdown() {
  logNotice("x0d", "Initiating quick shutdown.");
  daemon_->terminate();
}

void XzeroEventHandler::onGracefulShutdown() {
  logNotice("x0d", "Initiating graceful shutdown.");

  /* 1. stop all listeners
   * 2. wait until all requests have been handled.
   * 3. orderly shutdown
   */

  daemon_->server()->stop();
}

} // namespace x0d
