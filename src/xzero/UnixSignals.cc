// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/UnixSignals.h>
#include <xzero/UnixSignalInfo.h>
#include <xzero/executor/Executor.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/RuntimeError.h>
#include <xzero/Duration.h>
#include <xzero/logging.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <list>
#include <signal.h>

#if defined(XZERO_OS_LINUX)
#include <sys/signalfd.h>
#include <unistd.h>
#endif

#if defined(XZERO_OS_DARWIN)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

namespace xzero {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("UnixSignals", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

class SignalWatcher : public Executor::Handle {
 public:
  typedef Executor::Task Task;

  explicit SignalWatcher(UnixSignals::SignalHandler action)
      : action_(action) {};

  void fire() {
    Executor::Handle::fire(std::bind(action_, info));
  }

  UnixSignalInfo info;

 private:
  UnixSignals::SignalHandler action_;
};

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
    case SIGPOLL: return "SIGPOLL";
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

#if defined(XZERO_OS_LINUX) // {{{
class LinuxSignals : public UnixSignals {
 public:
  explicit LinuxSignals(Executor* executor);
  ~LinuxSignals();
  HandleRef executeOnSignal(int signo, SignalHandler task) override;
  void onSignal();

 private:
  Executor* executor_;
  Executor::HandleRef handle_;
  FileDescriptor fd_;
  sigset_t signalMask_;
  std::mutex mutex_;
  std::atomic<size_t> interests_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
};

LinuxSignals::LinuxSignals(Executor* executor)
    : executor_(executor),
      fd_(),
      signalMask_(),
      mutex_(),
      interests_(0),
      watchers_(128) {
  sigemptyset(&signalMask_);
}

LinuxSignals::~LinuxSignals() {
}

UnixSignals::HandleRef LinuxSignals::executeOnSignal(int signo,
                                                     SignalHandler task) {
  std::lock_guard<std::mutex> _l(mutex_);

  sigaddset(&signalMask_, signo);

  // on-demand create signalfd instance / update signalfd's mask
  if (fd_.isOpen()) {
    signalfd(fd_, &signalMask_, 0);
  } else {
    fd_ = signalfd(-1, &signalMask_, SFD_NONBLOCK | SFD_CLOEXEC);
    TRACE("executeOnSignal: signalfd=$0", fd_.get());
  }

  // block this signal also to avoid default disposition
  sigprocmask(SIG_BLOCK, &signalMask_, nullptr);

  TRACE("executeOnSignal: $0", toString(signo));
  RefPtr<SignalWatcher> hr(new SignalWatcher(task));
  watchers_[signo].emplace_back(hr);

  if (interests_.load() == 0) {
    handle_ = executor_->executeOnReadable(
        fd_,
        std::bind(&LinuxSignals::onSignal, this));
  }

  interests_++;

  return hr.as<Executor::Handle>();
}

void LinuxSignals::onSignal() {
  std::lock_guard<std::mutex> _l(mutex_);

  sigset_t clearMask;
  sigemptyset(&clearMask);

  signalfd_siginfo events[16];
  ssize_t n = 0;
  for (;;) {
    n = read(fd_, events, sizeof(events));
    if (n < 0) {
      if (errno == EINTR)
        continue;

      RAISE_ERRNO(errno);
    }
    break;
  }

  // move pending signals out of the watchers
  std::vector<RefPtr<SignalWatcher>> pending;
  n /= sizeof(*events);
  pending.reserve(n);
  {
    for (int i = 0; i < n; ++i) {
      const signalfd_siginfo& event = events[i];
      int signo = event.ssi_signo;
      std::list<RefPtr<SignalWatcher>>& watchers = watchers_[signo];

      logDebug("UnixSignals",
               "Caught signal $0 from PID $1 UID $2.",
               toString(signo),
               event.ssi_pid,
               event.ssi_uid);

      for (RefPtr<SignalWatcher>& watcher: watchers) {
        watcher->info.pid = event.ssi_pid;
        watcher->info.uid = event.ssi_uid;
      }

      sigdelset(&signalMask_, signo);
      interests_ -= watchers_[signo].size();
      pending.insert(pending.end(), watchers.begin(), watchers.end());
      watchers.clear();
    }

    // reregister for further signals, if anyone interested
    if (interests_.load() > 0) {
      executor_->executeOnReadable(
          fd_,
          std::bind(&LinuxSignals::onSignal, this));
    }
  }

  // update signal mask
  sigprocmask(SIG_BLOCK, &signalMask_, nullptr);
  signalfd(fd_, &signalMask_, 0);

  // notify interests (XXX must not be invoked on local stack)
  for (RefPtr<SignalWatcher>& hr: pending)
    executor_->execute(std::bind(&SignalWatcher::fire, hr));
}
#endif
// }}}
#if defined(XZERO_OS_DARWIN) // {{{
class KQueueSignals : public UnixSignals {
 public:
  explicit KQueueSignals(Executor* executor);
  ~KQueueSignals();
  HandleRef executeOnSignal(int signo, SignalHandler task) override;
  void onSignal();

 private:
  Executor* executor_;
  Executor::HandleRef handle_;
  FileDescriptor fd_;
  sigset_t oldSignalMask_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
  std::atomic<size_t> interests_;
  std::mutex mutex_;
};

KQueueSignals::KQueueSignals(Executor* executor)
    : executor_(executor),
      handle_(),
      fd_(kqueue()),
      oldSignalMask_(),
      watchers_(128),
      interests_(0),
      mutex_() {
  sigprocmask(SIG_SETMASK, nullptr, &oldSignalMask_);
}

KQueueSignals::~KQueueSignals() {
  if (handle_)
    handle_->cancel();

  // restore old signal mask
  sigprocmask(SIG_SETMASK, &oldSignalMask_, nullptr);
}

KQueueSignals::HandleRef KQueueSignals::executeOnSignal(int signo,
                                                        SignalHandler task) {
  std::lock_guard<std::mutex> _l(mutex_);

  // add event to kqueue
  if (watchers_[signo].empty()) {
    struct kevent ke;
    EV_SET(&ke, signo, EVFILT_SIGNAL, EV_ADD | EV_ONESHOT,
           0 /* fflags */,
           0 /* data */,
           nullptr /* udata */);
    int rv = kevent(fd_, &ke, 1, nullptr, 0, nullptr);
    if (rv < 0) {
      RAISE_ERRNO(errno);
    }

    blockSignal(signo);
  }

  RefPtr<SignalWatcher> hr(new SignalWatcher(task));
  watchers_[signo].emplace_back(hr);

  if (interests_.load() == 0) {
    handle_ = executor_->executeOnReadable(
        fd_,
        std::bind(&KQueueSignals::onSignal, this));
  }

  interests_++;

  return hr.as<Executor::Handle>();
}

void KQueueSignals::onSignal() {
  // collect pending signals
  timespec timeout;
  timeout.tv_sec = 0;
  timeout.tv_nsec = 0;
  struct kevent events[16];

  int rv = 0;
  for (;;) {
    rv = kevent(fd_, nullptr, 0, events, 128, &timeout);
    if (rv < 0) {
      if (errno == EINTR) {
        continue;
      }
      RAISE_ERRNO(errno);
    }
    break;
  }

  std::vector<RefPtr<SignalWatcher>> pending;
  pending.reserve(rv);
  {
    // move pending signals out of the watchers
    std::lock_guard<std::mutex> _l(mutex_);
    for (int i = 0; i < rv; ++i) {
      int signo = events[i].ident;
      std::list<RefPtr<SignalWatcher>>& watchers = watchers_[signo];

      logDebug("UnixSignals", "Caught signal $0.", toString(signo));

      pending.insert(pending.end(), watchers.begin(), watchers.end());
      interests_ -= watchers_[signo].size();
      watchers.clear();
    }

    // reregister for further signals, if anyone interested
    if (interests_.load() > 0) {
      handle_ = executor_->executeOnReadable(
          fd_,
          std::bind(&KQueueSignals::onSignal, this));
    }
  }

  // notify interests
  for (RefPtr<SignalWatcher>& hr: pending)
    executor_->execute(std::bind(&SignalWatcher::fire, hr));
}
#endif
// }}}

std::unique_ptr<UnixSignals> UnixSignals::create(Executor* executor) {
#if defined(XZERO_OS_LINUX)
  return std::unique_ptr<UnixSignals>(new LinuxSignals(executor));
#elif defined(XZERO_OS_DARWIN)
  return std::unique_ptr<UnixSignals>(new KQueueSignals(executor));
#else
#error "Unsupported platform."
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
