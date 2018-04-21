// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/ExceptionHandler.h>
#include <xzero/MonotonicClock.h>
#include <xzero/MonotonicTime.h>
#include <xzero/WallClock.h>
#include <xzero/executor/LinuxScheduler.h>
#include <xzero/logging.h>
#include <xzero/net/Socket.h>
#include <xzero/sysconfig.h>
#include <xzero/thread/Wakeup.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace xzero {

EventFd::EventFd()
    : handle_{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)} {
}

void EventFd::notify(uint64_t n) {
  ::write(handle_, &n, sizeof(n));
}

std::optional<uint64_t> EventFd::consume() {
  uint64_t counter = 0;
  if (::read(handle_, &counter, sizeof(counter)) < 0)
    return std::nullopt;
  else
    return counter;
}

#define EPOLL_MAX_USER_WATCHES_FILE "/proc/sys/fs/epoll/max_user_watches"

LinuxScheduler::LinuxScheduler(ExceptionHandler eh)
    : EventLoop(eh),
      lock_(),
      watchers_(),
      firstWatcher_(nullptr),
      lastWatcher_(nullptr),
      tasks_(),
      timers_(),
      epollfd_(),
      eventfd_(),
      signalLock_{},
      signalfd_(),
      signalMask_{},
      signalInterests_{0},
      signalWatchers_{128},
      now_(0),
      activeEvents_(1024),
      readerCount_(0),
      writerCount_(0),
      breakLoopCounter_(0)
{
  epollfd_ = epoll_create1(EPOLL_CLOEXEC);

  epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN;
  epoll_ctl(epollfd_, EPOLL_CTL_ADD, eventfd_.native(), &event);

  // setup signalfd
  sigemptyset(&signalMask_);
  signalfd_ = signalfd(-1, &signalMask_, SFD_NONBLOCK | SFD_CLOEXEC);
  if (signalfd_.isClosed() && errno != ENOSYS)
    RAISE_ERRNO(errno);
}

LinuxScheduler::LinuxScheduler()
    : LinuxScheduler(nullptr) {
}

LinuxScheduler::~LinuxScheduler() {
}

void LinuxScheduler::updateTime() {
  now_ = MonotonicClock::now();
}

MonotonicTime LinuxScheduler::now() const noexcept {
  return now_;
}

void LinuxScheduler::execute(Task task) {
  {
    std::lock_guard<std::mutex> lk(lock_);
    tasks_.emplace_back(std::move(task));
    ref();
  }
  wakeupLoop();
}

std::string LinuxScheduler::toString() const {
  return "LinuxScheduler";
}

Executor::HandleRef LinuxScheduler::executeAfter(Duration delay, Task task) {
  MonotonicTime time = MonotonicClock::now() + delay;
  return insertIntoTimersList(time, task);
}

Executor::HandleRef LinuxScheduler::executeAt(UnixTime when, Task task) {
  MonotonicTime time = now() + (when - WallClock::now());
  return insertIntoTimersList(time, task);
}

Executor::HandleRef LinuxScheduler::executeOnSignal(int signo, SignalHandler handler) {
  if (signalfd_.isClosed())
    return Executor::executeOnSignal(signo, handler);

  std::lock_guard<std::mutex> _l(signalLock_);

  sigaddset(&signalMask_, signo);

  int rv = signalfd(signalfd_, &signalMask_, 0);
  if (rv < 0) {
    RAISE_ERRNO(errno);
  }

  // block this signal also to avoid default disposition
  sigprocmask(SIG_BLOCK, &signalMask_, nullptr);

  auto hr = std::make_shared<SignalWatcher>(handler);
  signalWatchers_[signo].emplace_back(hr);

  if (signalInterests_.load() == 0) {
    executeOnReadable(signalfd_,
                      std::bind(&LinuxScheduler::onSignal, this),
                      5_years, nullptr);
    unref();
  }

  signalInterests_++;

  return hr;
}

void LinuxScheduler::onSignal() {
  std::lock_guard<std::mutex> _l(signalLock_);
  ref();

  sigset_t clearMask;
  sigemptyset(&clearMask);

  signalfd_siginfo events[16];
  ssize_t n = 0;
  for (;;) {
    n = read(signalfd_, events, sizeof(events));
    if (n < 0) {
      if (errno == EINTR)
        continue;

      RAISE_ERRNO(errno);
    }
    break;
  }

  // move pending signals out of the watchers
  std::vector<std::shared_ptr<SignalWatcher>> pending;
  n /= sizeof(*events);
  pending.reserve(n);
  {
    for (int i = 0; i < n; ++i) {
      const signalfd_siginfo& event = events[i];
      int signo = event.ssi_signo;
      std::list<std::shared_ptr<SignalWatcher>>& watchers = signalWatchers_[signo];

      logDebug("Caught signal {} from PID {} UID {}.",
               PosixSignals::toString(signo),
               event.ssi_pid,
               event.ssi_uid);

      for (std::shared_ptr<SignalWatcher>& watcher: watchers) {
        watcher->info.signal = signo;
        watcher->info.pid = event.ssi_pid;
        watcher->info.uid = event.ssi_uid;
      }

      sigdelset(&signalMask_, signo);
      signalInterests_ -= signalWatchers_[signo].size();
      pending.insert(pending.end(), watchers.begin(), watchers.end());
      watchers.clear();
    }

    // reregister for further signals, if anyone interested
    if (signalInterests_.load() > 0) {
      // TODO executeOnReadable(signalfd_, std::bind(&LinuxSignals::onSignal, this));
      unref();
    }
  }

  // update signal mask
  sigprocmask(SIG_BLOCK, &signalMask_, nullptr);
  signalfd(signalfd_, &signalMask_, 0);

  // notify interests (XXX must not be invoked on local stack)
  for (std::shared_ptr<SignalWatcher>& hr: pending)
    execute(std::bind(&SignalWatcher::fire, hr));
}

Executor::HandleRef LinuxScheduler::executeOnReadable(const Socket& s, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::READABLE, s.native(), task, tmo, tcb);
}

Executor::HandleRef LinuxScheduler::executeOnReadable(int fd, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::READABLE, fd, task, tmo, tcb);
}

Executor::HandleRef LinuxScheduler::executeOnWritable(const Socket& s, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::WRITABLE, s.native(), task, tmo, tcb);
}

Executor::HandleRef LinuxScheduler::executeOnWritable(int fd, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::WRITABLE, fd, task, tmo, tcb);
}

int LinuxScheduler::makeEvent(Mode mode) {
  switch (mode) {
    case Mode::READABLE:
      return EPOLLIN;
    case Mode::WRITABLE:
      return EPOLLOUT;
    default:
      logFatal("LinuxScheduler::makeEvent() received an illegal mode value.");
  }
}

void LinuxScheduler::wakeupLoop() {
  eventfd_.notify(1);
}

Executor::HandleRef LinuxScheduler::createWatcher(
    Mode mode, int fd, Task task, Duration timeout, Task tcb) {

  if (watchers_.find(fd) != watchers_.end()) {
    // RAISE(AlreadyWatchingOnResource, "Already watching on resource");
    logFatal("LinuxScheduler: Already watching on resource");
  }

  auto si = std::make_shared<Watcher>(fd, mode, task, MonotonicClock::now() + timeout, tcb);
  watchers_[fd] = si;
  Watcher* const interest = si.get();

  interest->setCancelHandler([this, interest] () {
    std::lock_guard<std::mutex> _l(lock_);
    unlinkWatcher(interest);
  });

  epoll_event event;
  memset(&event, 0, sizeof(event));
  event.data.ptr = interest;
  event.events = makeEvent(mode) | EPOLLONESHOT;

  int rv = epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event);
  if (rv < 0) {
    RAISE_ERRNO(errno);
  }

  switch (mode) {
    case Mode::READABLE:
      readerCount_++;
      break;
    case Mode::WRITABLE:
      writerCount_++;
      break;
  }

  return linkWatcher(interest, findPrecedingWatcher(interest));
}

LinuxScheduler::Watcher* LinuxScheduler::findPrecedingWatcher(Watcher* interest) {
  if (firstWatcher_ == nullptr)
    // put it in front (linked list empty currently anyway)
    return nullptr;

  // find closest predecessor in timeout-ASC-ordered linked list
  Watcher* succ = lastWatcher_;
  while (succ->prev != nullptr) {
    if (succ->prev->timeout < interest->timeout) {
      // put it after that
      return succ->prev;
    } else {
      succ = succ->prev;
    }
  }

  // the timeout of @p interest is smaller than anything else
  // in the timeout-ASC-ordered list (or list was just 1 item)
  if (firstWatcher_->timeout < interest->timeout) {
    // put in back
    return lastWatcher_;
  } else {
    // put in front
    return nullptr;
  }
}

LinuxScheduler::HandleRef LinuxScheduler::linkWatcher(Watcher* w, Watcher* pred) {
  Watcher* succ = pred ? pred->next : firstWatcher_;

  w->prev = pred;
  w->next = succ;

  if (pred)
    pred->next = w;
  else
    firstWatcher_ = w;

  if (succ)
    succ->prev = w;
  else
    lastWatcher_ = w;

  ref();

  wakeupLoop();

  return w->shared_from_this();
}

LinuxScheduler::Watcher* LinuxScheduler::unlinkWatcher(Watcher* w) {
  Watcher* pred = w->prev;
  Watcher* succ = w->next;

  epoll_ctl(epollfd_, EPOLL_CTL_DEL, w->fd, nullptr);

  if (pred)
    pred->next = succ;
  else
    firstWatcher_ = succ;

  if (succ)
    succ->prev = pred;
  else
    lastWatcher_ = pred;

  watchers_.erase(watchers_.find(w->fd));

  unref();

  return succ;
}

Executor::HandleRef LinuxScheduler::findWatcher(int fd) {
  std::lock_guard<std::mutex> lk(lock_);

  auto wi = watchers_.find(fd);
  if (wi != watchers_.end()) {
    return wi->second;
  } else {
    return nullptr;
  }
}

MonotonicTime LinuxScheduler::nextTimeout() const noexcept {
  if (!tasks_.empty())
    return now();

  const MonotonicTime a = !timers_.empty()
                        ? timers_.front()->when
                        : now() + 60_seconds;

  const MonotonicTime b = firstWatcher_ != nullptr
                        ? firstWatcher_->timeout
                        : now() + 61_seconds;

  return std::min(a, b);
}

void LinuxScheduler::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  wakeup->onWakeup(generation, std::bind(&LinuxScheduler::execute, this, task));
}

void LinuxScheduler::runLoop() {
  loop(true);
}

void LinuxScheduler::runLoopOnce() {
  loop(false);
}

void LinuxScheduler::loop(bool repeat) {
  if (referenceCount() == 0)
    return;

  breakLoopCounter_.store(0);

  do {
    size_t numEvents = waitForEvents();
    std::list<Task> activeTasks = collectEvents(numEvents);
    safeCallEach(activeTasks);
  } while (breakLoopCounter_.load() == 0 && repeat && referenceCount() > 0);
}

size_t LinuxScheduler::waitForEvents() noexcept {
  updateTime();

  const MonotonicTime timeout = nextTimeout();

  // XXX on Windows Subsystem for Linux (WSL), I found out, that
  // epoll_wait() (even with no watches active) is returning *before*
  // the timeout has been passed. Hence, I have to loop here
  // until either an event has been triggered or the timeout has
  // *truely* been reached.
  // In the tests, this loop some times needs 12 iterations. and around
  // half of the many tests needed re-iterating in order to hit the timeout.

  int rv = 0;
  do {
    const Duration maxWait = distance(now(), timeout);
    do rv = epoll_wait(epollfd_, &activeEvents_[0], activeEvents_.size(),
                       maxWait.milliseconds());
    while (rv < 0 && errno == EINTR);

    if (rv < 0)
      logFatal("epoll_wait returned unexpected error code: {}", strerror(errno));

    updateTime();
  } while (rv == 0 && timeout >= now());

  return static_cast<size_t>(rv);
}

std::list<EventLoop::Task> LinuxScheduler::collectEvents(size_t count) {
  std::lock_guard<std::mutex> lk(lock_);

  std::list<Task> activeTasks;

  unref(tasks_.size());
  activeTasks = std::move(tasks_);

  collectActiveHandles(count, &activeTasks);
  collectTimeouts(&activeTasks);

  return activeTasks;
}

void LinuxScheduler::breakLoop() {
  breakLoopCounter_++;
  wakeupLoop();
}

EventLoop::HandleRef LinuxScheduler::insertIntoTimersList(MonotonicTime dt,
                                                          Task task) {
  auto tref = std::make_shared<Timer>(dt, task);
  Timer* t = tref.get();

  t->setCancelHandler([this, t]() {
    std::lock_guard<std::mutex> _l(lock_);

    auto i = timers_.end();
    auto e = timers_.begin();

    while (i != e) {
      i--;
      if (i->get() == t) {
        timers_.erase(i);
        unref();
        break;
      }
    }
  });

  std::lock_guard<std::mutex> lk(lock_);

  ref();

  auto i = timers_.end();
  auto e = timers_.begin();

  while (i != e) {
    i--;
    const std::shared_ptr<Timer>& current = *i;
    if (t->when >= current->when) {
      i++;
      i = timers_.insert(i, tref);
      return tref;
    }
  }

  timers_.push_front(tref);
  i = timers_.begin();
  return tref;

  // return std::make_shared<Handle>([this, i]() {
  //   std::lock_guard<std::mutex> lk(lock_);
  //   timers_.erase(i);
  // });
}

void LinuxScheduler::collectActiveHandles(size_t count,
                                          std::list<Task>* result) {
  for (size_t i = 0; i < count; ++i) {
    epoll_event& event = activeEvents_[i];
    if (event.data.ptr) {
      auto w = std::static_pointer_cast<Watcher>(
          static_cast<Handle*>(event.data.ptr)->shared_from_this());
      if (event.events & EPOLLIN) {
        readerCount_--;
        result->push_back([w] { w->fire(w->onIO); });
        unlinkWatcher(w.get());
      } else if (event.events & EPOLLOUT) {
        writerCount_--;
        result->push_back([w] { w->fire(w->onIO); });
        unlinkWatcher(w.get());
      } else {
        // unknown event fd (w->fd)
      }
    } else {
      // event.data.ptr is NULL, ie. that's our eventfd notification.
      eventfd_.consume();
    }
  }
}

void LinuxScheduler::collectTimeouts(std::list<Task>* result) {
  const auto nextTimedout = [this]() -> std::shared_ptr<Watcher> {
    return firstWatcher_ && firstWatcher_->timeout <= now()
        ? std::static_pointer_cast<Watcher>(firstWatcher_->shared_from_this())
        : std::shared_ptr<Watcher>();
  };

  while (std::shared_ptr<Watcher> w = nextTimedout()) {
    switch (w->mode) {
      case Mode::READABLE: readerCount_--; break;
      case Mode::WRITABLE: writerCount_--; break;
    }
    result->push_back([w] { w->fire(w->onTimeout); });
    unlinkWatcher(w.get());
  }

  const auto nextTimer = [this]() -> std::shared_ptr<Timer> {
    return !timers_.empty() && timers_.front()->when <= now()
        ? std::static_pointer_cast<Timer>(timers_.front())
        : std::shared_ptr<Timer>{};
  };

  unsigned t = 0;
  while (std::shared_ptr<Timer> job = nextTimer()) {
    t++;
    result->push_back([job] { job->fire(job->action); });
    timers_.pop_front();
    unref();
  }
}

void LinuxScheduler::Watcher::clear() {
  fd = -1;
  // Mode mode;
  // Task onIO;
  // MonotonicTime timeout;
  // Task onTimeout;
  prev = nullptr;
  next = nullptr;
}

} // namespace xzero
