// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
      signals_(UnixSignals::create(executor)),
      executor_(executor),
      state_(XzeroState::Inactive) {

  signals_->notify(SIGHUP, std::bind(&XzeroEventHandler::onConfigReload, this, std::placeholders::_1));
  signals_->notify(SIGUSR1, std::bind(&XzeroEventHandler::onCycleLogs, this, std::placeholders::_1));
  signals_->notify(SIGUSR2, std::bind(&XzeroEventHandler::onUpgradeBinary, this, std::placeholders::_1));
  signals_->notify(SIGQUIT, std::bind(&XzeroEventHandler::onGracefulShutdown, this, std::placeholders::_1));
  signals_->notify(SIGTERM, std::bind(&XzeroEventHandler::onQuickShutdown, this, std::placeholders::_1));
  signals_->notify(SIGINT, std::bind(&XzeroEventHandler::onQuickShutdown, this, std::placeholders::_1));
}

XzeroEventHandler::~XzeroEventHandler() {
}

void XzeroEventHandler::onConfigReload(const xzero::UnixSignalInfo& info) {
  logNotice("x0d",
            "Reloading configuration. (requested via $0 by UID $1 PID $2)",
            UnixSignals::toString(info.signal),
            info.uid.getOrElse(-1),
            info.pid.getOrElse(-1));

  /* daemon_->reloadConfiguration(); */

  signals_->notify(SIGHUP, std::bind(&XzeroEventHandler::onConfigReload, this, std::placeholders::_1));
}

void XzeroEventHandler::onCycleLogs(const xzero::UnixSignalInfo& info) {
  logNotice("x0d", "Cycling logs. (requested via $0 by UID $1 PID $2)",
            UnixSignals::toString(info.signal),
            info.uid.getOrElse(-1),
            info.pid.getOrElse(-1));

  daemon_->onCycleLogs();

  signals_->notify(SIGUSR1, std::bind(&XzeroEventHandler::onCycleLogs, this, std::placeholders::_1));
}

void XzeroEventHandler::onUpgradeBinary(const UnixSignalInfo& info) {
  logNotice("x0d",
            "Upgrading binary. (requested via $0 by UID $1 PID $2)",
            UnixSignals::toString(info.signal),
            info.uid.getOrElse(-1),
            info.pid.getOrElse(-1));

  /* TODO [x0d] binary upgrade
   * 1. suspend the world
   * 2. save state into temporary file with an inheriting file descriptor
   * 3. exec into new binary
   * 4. (new process) load state from file descriptor and close fd
   * 5. (new process) resume the world
   */
}

void XzeroEventHandler::onQuickShutdown(const xzero::UnixSignalInfo& info) {
  logNotice("x0d",
            "Initiating quick shutdown. (requested via $0 by UID $1 PID $2)",
            UnixSignals::toString(info.signal),
            info.uid.getOrElse(-1),
            info.pid.getOrElse(-1));

  daemon_->terminate();
}

void XzeroEventHandler::onGracefulShutdown(const xzero::UnixSignalInfo& info) {
  logNotice("x0d",
            "Initiating graceful shutdown. (requested via $0 by UID $1 PID $2)",
            UnixSignals::toString(info.signal),
            info.uid.getOrElse(-1),
            info.pid.getOrElse(-1));

  /* 1. stop all listeners
   * 2. wait until all requests have been handled.
   * 3. orderly shutdown
   */

  daemon_->server()->stop();
}

} // namespace x0d
