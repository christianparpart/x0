// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/PosixSignals.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

#include <cassert>
#include <csignal>

namespace xzero {

class PosixSignals::SignalWatcher : public Executor::Handle {
 public:
  typedef Executor::Task Task;

  explicit SignalWatcher(SignalHandler action)
      : action_(action) {};

  void fire() {
    Executor::Handle::fire(std::bind(action_, info));
  }

  UnixSignalInfo info;

 private:
  SignalHandler action_;
};

PosixSignals* PosixSignals::singleton_ = nullptr;

PosixSignals::PosixSignals(Executor* executor)
    : executor_(executor), watchers_(128), mutex_() {
  assert(singleton_ == nullptr && "Must be a singleton.");
  singleton_ = this;
}

PosixSignals::~PosixSignals() {
  singleton_ = nullptr;
}

PosixSignals::HandleRef PosixSignals::notify(int signo, SignalHandler task) {
  std::lock_guard<std::mutex> _l(mutex_);

  std::shared_ptr<SignalWatcher> hr = std::make_shared<SignalWatcher>(task);

  if (watchers_[signo].empty()) {
#if defined(XZERO_OS_WIN32)
    signal(signo, &PosixSignals::onSignal);
#else
    struct sigaction sa{};
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = PosixSignals::onSignal;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(signo, &sa, nullptr);
#endif
  }

  watchers_[signo].emplace_back(hr);

  return hr;
}

#if defined(XZERO_OS_WIN32)
void PosixSignals::onSignal(int signo) {
  singleton_->onSignal2(signo, 0, 0, nullptr);
}
#else
void PosixSignals::onSignal(int signo, siginfo_t* info, void* ptr) {
  singleton_->onSignal2(signo, info->si_pid, info->si_uid, ptr);
}
#endif

void PosixSignals::onSignal2(int signo, int pid, int uid, void* ptr) {
  std::vector<std::shared_ptr<SignalWatcher>> pending;
  {
    std::lock_guard<std::mutex> _l(mutex_);
    std::list<std::shared_ptr<SignalWatcher>>& watchers = watchers_[signo];
    pending.reserve(watchers.size());
    pending.insert(pending.end(), watchers.begin(), watchers.end());
    watchers.clear();
  }

  for (std::shared_ptr<SignalWatcher>& p: pending) {
    p->info.signal = signo;
    p->info.pid = pid;
    p->info.uid = uid;
  }

  // notify interests (XXX must not be invoked on local stack)
  for (const auto& p: pending)
    executor_->execute(std::bind(&SignalWatcher::fire, p));
}

PosixSignals* PosixSignals::get() {
  return singleton_;
}

void PosixSignals::blockSignal(int signo) {
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

void PosixSignals::unblockSignal(int signo) {
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

std::string PosixSignals::toString(int signo) {
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

  return fmt::format("<{}>", signo);
}

}  // namespace xzero
