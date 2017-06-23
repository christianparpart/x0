// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/UnixSignals.h>
#include <xzero/PosixSignals.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>
#include <memory>
#include <signal.h>

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
    case SIGHUP: return "SIGHUP";
    case SIGINT: return "SIGINT";
    case SIGQUIT: return "SIGQUIT";
    case SIGILL: return "SIGILL";
    case SIGABRT: return "SIGABRT";
    case SIGFPE: return "SIGFPE";
    /* SIGKILL: cannot be trapped */
    case SIGSEGV: return "SIGSEGV";
    case SIGPIPE: return "SIGPIPE";
    case SIGALRM: return "SIGALRM";
    case SIGTERM: return "SIGTERM";
    case SIGUSR1: return "SIGUSR1";
    case SIGUSR2: return "SIGUSR2";
    case SIGCHLD: return "SIGCHLD";
    case SIGCONT: return "SIGCONT";
    /* SIGSTOP: cannot be trapped */
    // XXX POSIX.1-2001
    case SIGTSTP: return "SIGTSTP";
    case SIGTTIN: return "SIGTTIN";
    case SIGTTOU: return "SIGTTOU";
    case SIGBUS: return "SIGBUS";
#if defined(SIGIO)
    case SIGIO: return "SIGIO";
#endif
    case SIGPROF: return "SIGPROF";
    case SIGSYS: return "SIGSYS";
    case SIGTRAP: return "SIGTRAP";
    case SIGURG: return "SIGURG";
    case SIGVTALRM: return "SIGVTALRM";
    case SIGXCPU: return "SIGXCPU";
    case SIGXFSZ: return "SIGXFSZ";
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
#if defined(XZERO_WSL)
  // WSL doesn't support signalfd() yet (2017-06-19)
  return std::unique_ptr<UnixSignals>(new PosixSignals(executor));
#elif defined(XZERO_OS_LINUX)
  return std::unique_ptr<UnixSignals>(new LinuxSignals(executor));
#elif defined(XZERO_OS_DARWIN)
  return std::unique_ptr<UnixSignals>(new KQueueSignals(executor));
#else
  return std::unique_ptr<UnixSignals>(new PosixSignals(executor));
#endif
}

void UnixSignals::blockSignal(int signo) {
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signo);
  int rv = sigprocmask(SIG_BLOCK, &sigset, nullptr);
  if (rv < 0)
    RAISE_ERRNO(errno);
}

void UnixSignals::unblockSignal(int signo) {
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, signo);
  int rv = sigprocmask(SIG_UNBLOCK, &sigset, nullptr);
  if (rv < 0)
    RAISE_ERRNO(errno);
}

} // namespace xzero
