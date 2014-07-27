// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/sysconfig.h>
#include <x0d/XzeroEventHandler.h>
#include <x0d/XzeroDaemon.h>
#include <x0d/sysconfig.h>
#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <base/io/SyslogSink.h>
#include <base/Tokenizer.h>
#include <base/Logger.h>
#include <base/DateTime.h>
#include <base/strutils.h>
#include <base/Severity.h>
#include <base/DebugLogger.h>

#include <ev++.h>
#include <systemd/sd-daemon.h>

#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#define SIG_X0_SUSPEND (SIGRTMIN + 4)
#define SIG_X0_RESUME (SIGRTMIN + 5)

namespace x0d {

using namespace base;
using namespace xzero;

XzeroEventHandler::XzeroEventHandler(XzeroDaemon* daemon, ev::loop_ref loop)
    : daemon_(daemon),
      loop_(loop),
      state_(XzeroState::Inactive),
      terminateSignal_(loop_),
      ctrlcSignal_(loop_),
      quitSignal_(loop_),
      user1Signal_(loop_),
      hupSignal_(loop_),
      suspendSignal_(loop_),
      resumeSignal_(loop_),
      logLevelIncSignal_(loop_),
      logLevelDecSignal_(loop_),
      terminationTimeout_(loop_),
      child_(loop_) {
  setState(XzeroState::Initializing);

  terminateSignal_
      .set<XzeroEventHandler, &XzeroEventHandler::quickShutdownHandler>(this);
  terminateSignal_.start(SIGTERM);
  ev_unref(loop_);

  ctrlcSignal_.set<XzeroEventHandler, &XzeroEventHandler::quickShutdownHandler>(
      this);
  ctrlcSignal_.start(SIGINT);
  ev_unref(loop_);

  quitSignal_.set<XzeroEventHandler,
                  &XzeroEventHandler::gracefulShutdownHandler>(this);
  quitSignal_.start(SIGQUIT);
  ev_unref(loop_);

  user1Signal_.set<XzeroEventHandler, &XzeroEventHandler::reopenLogsHandler>(
      this);
  user1Signal_.start(SIGUSR1);
  ev_unref(loop_);

  hupSignal_.set<XzeroEventHandler, &XzeroEventHandler::reexecHandler>(this);
  hupSignal_.start(SIGHUP);
  ev_unref(loop_);

  suspendSignal_.set<XzeroEventHandler, &XzeroEventHandler::suspendHandler>(
      this);
  suspendSignal_.start(SIG_X0_SUSPEND);
  ev_unref(loop_);

  resumeSignal_.set<XzeroEventHandler, &XzeroEventHandler::resumeHandler>(this);
  resumeSignal_.start(SIG_X0_RESUME);
  ev_unref(loop_);

  logLevelIncSignal_.set<XzeroEventHandler, &XzeroEventHandler::logLevelInc>(
      this);
  logLevelIncSignal_.start(SIGTTIN);
  ev_unref(loop_);

  logLevelDecSignal_.set<XzeroEventHandler, &XzeroEventHandler::logLevelDec>(
      this);
  logLevelDecSignal_.start(SIGTTOU);
  ev_unref(loop_);
}

XzeroEventHandler::~XzeroEventHandler() {
  if (terminationTimeout_.is_active()) {
    ev_ref(loop_);
    terminationTimeout_.stop();
  }

  if (terminateSignal_.is_active()) {
    ev_ref(loop_);
    terminateSignal_.stop();
  }

  if (ctrlcSignal_.is_active()) {
    ev_ref(loop_);
    ctrlcSignal_.stop();
  }

  if (quitSignal_.is_active()) {
    ev_ref(loop_);
    quitSignal_.stop();
  }

  if (user1Signal_.is_active()) {
    ev_ref(loop_);
    user1Signal_.stop();
  }

  if (hupSignal_.is_active()) {
    ev_ref(loop_);
    hupSignal_.stop();
  }

  if (suspendSignal_.is_active()) {
    ev_ref(loop_);
    suspendSignal_.stop();
  }

  if (resumeSignal_.is_active()) {
    ev_ref(loop_);
    resumeSignal_.stop();
  }

  if (logLevelIncSignal_.is_active()) {
    ev_ref(loop_);
    logLevelIncSignal_.stop();
  }

  if (logLevelDecSignal_.is_active()) {
    ev_ref(loop_);
    logLevelDecSignal_.stop();
  }
}

HttpServer* XzeroEventHandler::server() const { return daemon_->server(); }

/*! Updates state change, and tell supervisors about our state change.
 */
void XzeroEventHandler::setState(XzeroState newState) {
  if (state_ == newState) {
    // most probabely a bug
  }

  switch (newState) {
    case XzeroState::Inactive:
      break;
    case XzeroState::Initializing:
#if defined(SD_FOUND)
      sd_notify(0, "STATUS=Initializing ...");
#endif
      break;
    case XzeroState::Running:
      if (server()->generation() == 1) {
        // we have been started up directoy (e.g. by systemd)
#if defined(SD_FOUND)
        sd_notifyf(0,
                   "MAINPID=%d\n"
                   "STATUS=Accepting requests ...\n"
                   "READY=1\n",
                   getpid());
#endif
      } else {
        // we have been invoked by x0d itself, e.g. a executable upgrade and/or
        // configuration reload.
        // Tell the parent-x0d to shutdown gracefully.
        // On receive, the parent process will tell systemd, that we are the new
        // master.
        ::kill(getppid(), SIGQUIT);
      }
      break;
    case XzeroState::Upgrading:
#if defined(SD_FOUND)
      sd_notify(0, "STATUS=Upgrading");
#endif
      server()->log(Severity::info, "Upgrading ...");
      break;
    case XzeroState::GracefullyShuttingdown:
      if (state_ == XzeroState::Running) {
#if defined(SD_FOUND)
        sd_notify(0, "STATUS=Shutting down gracefully ...");
#endif
      } else if (state_ == XzeroState::Upgrading) {
        // we're not the master anymore
        // tell systemd, that our freshly spawned child is taking over, and the
        // new master
        // XXX as of systemd v28, RELOADED=1 is not yet implemented, but on
        // their TODO list
#if defined(SD_FOUND)
        sd_notifyf(0,
                   "MAINPID=%d\n"
                   "STATUS=Accepting requests ...\n"
                   "RELOADED=1\n",
                   child_.pid);
#endif
      }
      break;
    default:
      // must be a bug
      break;
  }

  state_ = newState;
}

void XzeroEventHandler::reexecHandler(ev::sig& sig, int) { daemon_->reexec(); }

void XzeroEventHandler::reopenLogsHandler(ev::sig&, int) {
  server()->log(Severity::info, "Reopening of all log files requested.");
  daemon_->cycleLogs();
}

/** temporarily suspends processing new and currently active connections.
 */
void XzeroEventHandler::suspendHandler(ev::sig& sig, int) {
  // suspend worker threads while performing the reexec
  for (HttpWorker* worker : server()->workers()) {
    worker->suspend();
  }

  for (ServerSocket* listener : server()->listeners()) {
    // stop accepting new connections
    listener->stop();
  }
}

/** resumes previousely suspended execution.
 */
void XzeroEventHandler::resumeHandler(ev::sig& sig, int) {
  server()->log(Severity::trace, "Siganl %s received.",
                strsignal(sig.signum));

  server()->log(Severity::trace, "Resuming worker threads.");
  for (HttpWorker* worker : server()->workers()) {
    worker->resume();
  }
}

// stage-1 termination handler
void XzeroEventHandler::gracefulShutdownHandler(ev::sig& sig, int) {
  server()->log(Severity::info, "%s received. Shutting down gracefully.",
                strsignal(sig.signum));

  for (ServerSocket* listener : server()->listeners()) listener->close();

  if (state_ == XzeroState::Upgrading) {
    child_.stop();

    for (HttpWorker* worker : server()->workers()) {
      worker->resume();
    }
  }
  setState(XzeroState::GracefullyShuttingdown);

  // initiate graceful server-stop
  server()->maxKeepAlive = TimeSpan::Zero;
  server()->stop();
}

// stage-2 termination handler
void XzeroEventHandler::quickShutdownHandler(ev::sig& sig, int) {
  daemon_->log(Severity::info, "%s received. shutting down NOW.",
               strsignal(sig.signum));

  if (state_ != XzeroState::Upgrading) {
    // we are no garbage parent process
#if defined(SD_FOUND)
    sd_notify(0, "STATUS=Shutting down.");
#endif
  }

  // default to standard signal-handler
  ev_ref(loop_);
  sig.stop();

  // install shutdown timeout handler
  terminationTimeout_
      .set<XzeroEventHandler, &XzeroEventHandler::quickShutdownTimeout>(this);
  terminationTimeout_.start(10, 0);
  ev_unref(loop_);

  // kill active HTTP connections
  server()->kill();
}

void XzeroEventHandler::quickShutdownTimeout(ev::timer&, int) {
  daemon_->log(Severity::warn, "Quick shutdown timed out. Terminating.");

  ev_ref(loop_);
  terminationTimeout_.stop();

  ev_break(loop_, ev::ALL);
}

/**
 * Watches over x0d-fork.
 * \param pid Child process ID.
 *
 */
void XzeroEventHandler::setupChild(int pid) {
  child_.set<XzeroEventHandler, &XzeroEventHandler::onChild>(this);
  child_.set(pid, 0);
  child_.start();
}

void XzeroEventHandler::onChild(ev::child&, int) {
  // the child exited before we receive a SUCCESS from it. so resume normal
  // operation again.
  server()->log(Severity::error,
                "New process exited with %d. Resuming normal operation.");

  child_.stop();

  // reenable HUP-signal
  if (!hupSignal_.is_active()) {
    server()->log(Severity::error, "Reenable HUP-signal.");
    hupSignal_.start();
    ev_unref(loop_);
  }

  server()->log(Severity::trace, "Reactivating listeners.");
  for (ServerSocket* listener : server()->listeners()) {
    // reenable O_CLOEXEC on listener socket
    listener->setCloseOnExec(true);

    // start accepting new connections
    listener->start();
  }

  server()->log(Severity::trace, "Resuming workers.");
  for (HttpWorker* worker : server()->workers()) {
    worker->resume();
  }
}

void XzeroEventHandler::logLevelInc(ev::sig& sig, int) {
  Severity s = server()->logLevel();
  if (s > 0) s = s - 1;

  server()->setLogLevel(s);
}

void XzeroEventHandler::logLevelDec(ev::sig& sig, int) {
  Severity s = server()->logLevel();
  if (s < Severity::emerg) s = s + 1;

  server()->setLogLevel(s);
}

}  // namespace x0d
