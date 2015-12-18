// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/UnixSignals.h>
#include <xzero/executor/Executor.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/RuntimeError.h>
#include <xzero/Duration.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <list>

#if defined(XZERO_OS_LINUX)
#include <linux/epoll.h>
#endif

#if defined(XZERO_OS_DARWIN)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

namespace xzero {

class SignalWatcher : public Executor::Handle {
 public:
  typedef Executor::Task Task;

  explicit SignalWatcher(Task action) : action_(action) {};

  void fire() { Executor::Handle::fire(action_); }

 private:
  Task action_;
};

#if defined(XZERO_OS_LINUX) // {{{
class LinuxSignals : public UnixSignals {
 public:
  explicit LinuxSignals(Executor* executor);
  ~LinuxSignals();
  HandleRef executeOnSignal(int signo, Task task) override;

 private:
  Executor* executor_;
  FileDescriptor fd_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
};
#endif
// }}}
#if defined(XZERO_OS_DARWIN) // {{{
class KQueueSignals : public UnixSignals {
 public:
  explicit KQueueSignals(Executor* executor);
  ~KQueueSignals();
  HandleRef executeOnSignal(int signo, Task task) override;
  void onSignal();

 private:
  Executor* executor_;
  Executor::HandleRef handle_;
  FileDescriptor fd_;
  std::vector<std::list<RefPtr<SignalWatcher>>> watchers_;
  std::atomic<size_t> interests_;
  std::mutex mutex_;
};

KQueueSignals::KQueueSignals(Executor* executor)
    : executor_(executor),
      handle_(),
      fd_(kqueue()),
      watchers_(128),
      interests_(0),
      mutex_() {
}

KQueueSignals::~KQueueSignals() {
  if (handle_) {
    handle_->cancel();
  }
}

KQueueSignals::HandleRef KQueueSignals::executeOnSignal(int signo, Task task) {
  std::lock_guard<std::mutex> _l(mutex_);

  // add event to kqueue
  if (watchers_[signo].empty()) {
    struct kevent ke;
    uintptr_t ident = signo;
    int16_t filter = EVFILT_SIGNAL;
    uint16_t flags = EV_ADD | EV_ONESHOT;
    uint32_t fflags = 0;
    intptr_t data = 0;
    void* udata = nullptr;
    EV_SET(&ke, ident, filter, flags, fflags, data, udata);
    int rv = kevent(fd_, &ke, 1, nullptr, 0, nullptr);
    if (rv < 0) {
      RAISE_ERRNO(errno);
    }
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
  int rv = kevent(fd_, nullptr, 0, events, 128, &timeout);
  if (rv < 0)
    RAISE_ERRNO(errno);

  // move pending signals out of the watchers
  std::vector<RefPtr<SignalWatcher>> pending(rv);
  {
    std::lock_guard<std::mutex> _l(mutex_);
    for (int i = 0; i < rv; ++i) {
      int signo = events[i].ident;
      for (RefPtr<SignalWatcher> sig: watchers_[signo]) {
        pending.emplace_back(sig);
      }
      watchers_[signo].clear();
    }
  }

  // notify interests
  for (RefPtr<SignalWatcher>& hr: pending)
    executor_->execute(std::bind(&SignalWatcher::fire, hr));

  // reregister for further signals, if anyone interested
  if (interests_.load() > 0) {
    handle_ = executor_->executeOnReadable(
        fd_,
        std::bind(&KQueueSignals::onSignal, this));
  }
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

} // namespace xzero
