// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/UnixSignals.h>
#include <xzero/Application.h>
#include <xzero/PosixSignals.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>
#include <xzero/logging.h>
#include <memory>
#include <csignal>

#if defined(XZERO_OS_LINUX)
#include <xzero/LinuxSignals.h>
#endif

#if defined(XZERO_OS_DARWIN)
#include <xzero/KQueueSignals.h>
#endif

namespace xzero {

std::string UnixSignals::toString(int signo) {
  switch (signo) {
    // XXX POSIX.1-1990
#if defined(SIGHUP)
    case SIGHUP: return "SIGHUP";
#endif
    case SIGINT: return "SIGINT";
#if defined(SIGQUIT)
    case SIGQUIT: return "SIGQUIT";
#endif
    case SIGILL: return "SIGILL";
    case SIGABRT: return "SIGABRT";
    case SIGFPE: return "SIGFPE";
    /* SIGKILL: cannot be trapped */
    case SIGSEGV: return "SIGSEGV";
#if defined(SIGPIPE)
    case SIGPIPE: return "SIGPIPE";
#endif
#if defined(SIGALRM)
    case SIGALRM: return "SIGALRM";
#endif
    case SIGTERM: return "SIGTERM";
#if defined(SIGUSR1)
    case SIGUSR1: return "SIGUSR1";
#endif
#if defined(SIGUSR2)
    case SIGUSR2: return "SIGUSR2";
#endif
#if defined(SIGCHLD)
    case SIGCHLD: return "SIGCHLD";
#endif
#if defined(SIGCONT)
    case SIGCONT: return "SIGCONT";
#endif
    /* SIGSTOP: cannot be trapped */
    // XXX POSIX.1-2001
#if defined(SIGSTP)
    case SIGTSTP: return "SIGTSTP";
#endif
#if defined(SIGTTIN)
    case SIGTTIN: return "SIGTTIN";
#endif
#if defined(SIGTTOU)
    case SIGTTOU: return "SIGTTOU";
#endif
#if defined(SIGBUS)
    case SIGBUS: return "SIGBUS";
#endif
#if defined(SIGIO)
    case SIGIO: return "SIGIO";
#endif
#if defined(SIGPROF)
    case SIGPROF: return "SIGPROF";
#endif
#if defined(SIGSYS)
    case SIGSYS: return "SIGSYS";
#endif
#if defined(SIGTRAP)
    case SIGTRAP: return "SIGTRAP";
#endif
#if defined(SIGURG)
    case SIGURG: return "SIGURG";
#endif
#if defined(SIGVTALRM)
    case SIGVTALRM: return "SIGVTALRM";
#endif
#if defined(SIGXCPU)
    case SIGXCPU: return "SIGXCPU";
#endif
#if defined(SIGXFSZ)
    case SIGXFSZ: return "SIGXFSZ";
#endif
    // XXX various other signals
#if defined(SIGPWR)
    case SIGPWR: return "SIGPWR";
#endif
#if defined(SIGWINCH)
    case SIGWINCH: return "SIGWINCH";
#endif
    default: break;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "<%d>", signo);
  return buf;
}

std::unique_ptr<UnixSignals> UnixSignals::create(Executor* executor) {
#if defined(XZERO_OS_LINUX)
  if (Application::isWSL()) {
    // WSL doesn't support signalfd() yet (2018-03-31)
    return std::make_unique<PosixSignals>(executor);
  } else {
    return std::make_unique<LinuxSignals>(executor);
  }
#elif defined(XZERO_OS_DARWIN)
  return std::make_unique<KQueueSignals>(executor);
#else
  return std::make_unique<PosixSignals>(executor);
#endif
}

void UnixSignals::blockSignal(int signo) {
#if defined(HAVE_SIGPROCMASK)
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signo);
  int rv = sigprocmask(SIG_BLOCK, &sigset, nullptr);
  if (rv < 0)
    RAISE_ERRNO(errno);
#else
  logFatal("Not Implemented");
#endif
}

void UnixSignals::unblockSignal(int signo) {
#if defined(HAVE_SIGPROCMASK)
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signo);
  int rv = sigprocmask(SIG_UNBLOCK, &sigset, nullptr);
  if (rv < 0)
    RAISE_ERRNO(errno);
#else
  logFatal("Not Implemented");
#endif
}

} // namespace xzero
