// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/LinuxScheduler.h>
#include <xzero/thread/Wakeup.h>
#include <xzero/MonotonicTime.h>
#include <xzero/MonotonicClock.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/ExceptionHandler.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <algorithm>
#include <iostream>
#include <vector>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace xzero {

#define EPOLL_MAX_USER_WATCHES_FILE "/proc/sys/fs/epoll/max_user_watches"

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("LinuxScheduler: " msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

LinuxScheduler::LinuxScheduler(
    ExceptionHandler eh,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : EventLoop(eh),
      onPreInvokePending_(preInvoke),
      onPostInvokePending_(postInvoke),
      lock_(),
      watchers_(),
      firstWatcher_(nullptr),
      lastWatcher_(nullptr),
      tasks_(),
      timers_(),
      epollfd_(),
      eventfd_(),
      signalfd_(),
      now_(0),
      activeEvents_(1024),
      readerCount_(0),
      writerCount_(0),
      breakLoopCounter_(0)
{
  TRACE("LinuxScheduler()");
  epollfd_ = epoll_create1(EPOLL_CLOEXEC);

  eventfd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = EPOLLIN;
  epoll_ctl(epollfd_, EPOLL_CTL_ADD, eventfd_, &event);

  //TODO signalfd_ = ...;
}

LinuxScheduler::LinuxScheduler(ExceptionHandler eh)
    : LinuxScheduler(eh, nullptr, nullptr) {
}

LinuxScheduler::LinuxScheduler()
    : LinuxScheduler(nullptr, nullptr, nullptr) {
}

LinuxScheduler::~LinuxScheduler() {
}

void LinuxScheduler::updateTime() {
  now_ = MonotonicClock::now();
  TRACE("updateTime: now $0", now_);
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
  TRACE("executeAfter: delay=$0, abs=$1", delay, time);
  return insertIntoTimersList(time, task);
}

Executor::HandleRef LinuxScheduler::executeAt(UnixTime when, Task task) {
  MonotonicTime time = now() + (when - WallClock::now());
  TRACE("executeAt: $0", time);
  return insertIntoTimersList(time, task);
}

Executor::HandleRef LinuxScheduler::executeOnReadable(int fd, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::READABLE, fd, task, tmo, tcb);
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
      RAISE_STATUS(InvalidArgumentError);
  }
}

void LinuxScheduler::wakeupLoop() {
  uint64_t dummy = 1;
  ::write(eventfd_, &dummy, sizeof(dummy));
}

Executor::HandleRef LinuxScheduler::createWatcher(
    Mode mode, int fd, Task task, Duration timeout, Task tcb) {
  TRACE("createWatcher(fd: $0, mode: $1, timeout: $2)", fd, mode, timeout);

  if (watchers_.find(fd) != watchers_.end()) {
    // RAISE(AlreadyWatchingOnResource, "Already watching on resource");
    logFatal("LinuxScheduler: Already watching on resource");
  }

  Watcher* interest = new Watcher(fd, mode, task, MonotonicClock::now() + timeout, tcb);
  watchers_[fd] = std::shared_ptr<Watcher>(interest);

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
    TRACE("epoll_ctl for fd $0 failed #1. $1", fd, strerror(errno));
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
  TRACE("findPrecedingWatcher for $0", interest);

  for (Watcher* w = firstWatcher_; w != nullptr; w = w->next) {
    TRACE(" - $0", w);
  }

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
  TRACE("linkWatcher: w: $0, pred: $1", w, pred);
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

  TRACE("unlinkWatcher: $0", w);

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

void LinuxScheduler::cancelFD(int fd) {
  HandleRef ref = findWatcher(fd);
  if (ref != nullptr) {
    ref->cancel();
  }
}

void LinuxScheduler::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  wakeup->onWakeup(generation, std::bind(&LinuxScheduler::execute, this, task));
}

void LinuxScheduler::runLoop() {
  breakLoopCounter_.store(0);
  loop(true);
}

void LinuxScheduler::runLoopOnce() {
  loop(false);
}

void LinuxScheduler::loop(bool repeat) {
  while (referenceCount() > 0 && breakLoopCounter_.load() == 0) {
    size_t numEvents = waitForEvents();
    std::list<Task> activeTasks = collectEvents(numEvents);
    safeCall(onPreInvokePending_);
    safeCallEach(activeTasks);
    safeCall(onPostInvokePending_);

    if (!repeat) {
      break;
    }
  }
}

size_t LinuxScheduler::waitForEvents() noexcept {
  updateTime();

  const MonotonicTime timeout = nextTimeout();

  TRACE("waitForEvents: referenceCount=$0, breakLoopCounter=$1, timeout=$2",
        referenceCount(), breakLoopCounter_.load(),
        timeout);

  // XXX on Windows Subsystem for Linux (WSL), I found out, that
  // epoll_wait() (even with no watches active) is returning *before*
  // the timeout has been passed. Hence, I have to loop here
  // until either an event has been triggered or the timeout has
  // *truely* been reached.
  // In the tests, this loop some times needs 12 iterations. and around
  // half of the many tests needed re-iterating in order to hit the timeout.

  int rv = 0;
  do {
    const Duration maxWait = timeout - now();
    do rv = epoll_wait(epollfd_, &activeEvents_[0], activeEvents_.size(),
                       maxWait.milliseconds());
    while (rv < 0 && errno == EINTR);

    TRACE("waitForEvents: epoll_wait(fd=$0, count=$1, timeout=$2) = $3 $4",
          epollfd_.get(), activeEvents_.size(), maxWait, rv,
          rv < 0 ? strerror(rv) : "");

    if (rv < 0)
      logFatal("epoll_wait returned unexpected error code: $0", strerror(errno));

    updateTime();
  } while (rv == 0 && timeout >= now());

  TRACE("waitForEvents: got $0 events", rv);
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
  TRACE("breakLoop()");
  breakLoopCounter_++;
  wakeupLoop();
}

EventLoop::HandleRef LinuxScheduler::insertIntoTimersList(MonotonicTime dt,
                                                          Task task) {
  TRACE("insertIntoTimersList() $0", dt);
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
        TRACE("collectActiveHandles: $0 READABLE", w->fd);
        readerCount_--;
        result->push_back([w] { w->fire(w->onIO); });
        unlinkWatcher(w.get());
      } else if (event.events & EPOLLOUT) {
        TRACE("collectActiveHandles: $0 WRITABLE", w->fd);
        writerCount_--;
        result->push_back([w] { w->fire(w->onIO); });
        unlinkWatcher(w.get());
      } else {
        TRACE("collectActiveHandles: unknown event fd $0", w->fd);
      }
    } else {
      // event.data.ptr is NULL, ie. that's our eventfd notification.
      uint64_t counter = 0;
      ::read(eventfd_, &counter, sizeof(counter));
      TRACE("collectActiveHandles: eventfd consumed ($0)", counter);
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
    TRACE("collectTimeouts: i/o fd=$0, mode=$1", w->fd, w->mode);
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
    TRACE("collectTimeouts: collecting job at time $0", job->when);
    t++;
    result->push_back([job] { job->fire(job->action); });
    timers_.pop_front();
    unref();
  }
  TRACE("collectTimeouts: $0 timeouts collected", t);
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

std::ostream& operator<<(std::ostream& os, LinuxScheduler::Mode m) {
  if ((int)m & ((int)LinuxScheduler::Mode::READABLE | (int)LinuxScheduler::Mode::WRITABLE))
    return os << "READABLE|WRITABLE";
  else if ((int)m & (int)LinuxScheduler::Mode::READABLE)
    return os << "READABLE";
  else if ((int)m & (int)LinuxScheduler::Mode::WRITABLE)
    return os << "WRITABLE";
  else
    return os << "0";
}

std::ostream& operator<<(std::ostream& os, LinuxScheduler::Watcher* w) {
  if (w != nullptr)
    return os << StringUtil::format("{fd: $0/$1, timeout: $2}",
                                    w->fd, w->mode, w->timeout);
  else
    return os << "NULL";
}

} // namespace xzero
