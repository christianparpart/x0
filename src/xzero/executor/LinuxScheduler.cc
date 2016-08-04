// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
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
#include <vector>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace xzero {

#define EPOLL_MAX_USER_WATCHES_FILE "/proc/sys/fs/epoll/max_user_watches"

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("LinuxScheduler", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

LinuxScheduler::LinuxScheduler(
    std::unique_ptr<xzero::ExceptionHandler> eh,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : EventLoop(std::move(eh)),
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
      timerfd_(),
      signalfd_(),
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

  //TODO timerfd_ = ..;
  //TODO signalfd_ = ...;
}

LinuxScheduler::LinuxScheduler(
    std::unique_ptr<xzero::ExceptionHandler> eh)
    : LinuxScheduler(std::move(eh), nullptr, nullptr) {
}

LinuxScheduler::LinuxScheduler()
    : LinuxScheduler(nullptr, nullptr, nullptr) {
}

LinuxScheduler::~LinuxScheduler() {
}

MonotonicTime LinuxScheduler::now() const {
  return MonotonicClock::now();
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
  TRACE("executeAfter: $0", delay);
  return insertIntoTimersList(now() + delay, task);
}

Executor::HandleRef LinuxScheduler::executeAt(UnixTime when, Task task) {
  return insertIntoTimersList(now() + (when - WallClock::now()), task);
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
  }
}

void LinuxScheduler::wakeupLoop() {
  int dummy = 1;
  ::write(eventfd_, &dummy, sizeof(dummy));
}

Executor::HandleRef LinuxScheduler::createWatcher(
    Mode mode, int fd, Task task, Duration tmo, Task tcb) {
  TRACE("createWatcher(fd: $0, mode: $1, timeout: $2)", fd, mode, tmo);

  MonotonicTime timeout = now() + tmo;

  std::unordered_map<int, Watcher>::iterator wi = watchers_.find(fd);
  if (wi != watchers_.end() && wi->second.fd >= 0) {
    RAISE(AlreadyWatchingOnResource, "Already watching on resource");
  }

  Watcher* interest = &watchers_[fd];

  interest->reset(fd, mode, task, now() + tmo, tcb);

  interest->setCancelHandler([this, interest] () {
    std::lock_guard<std::mutex> _l(lock_);
    unlinkWatcher(interest);
  });

  epoll_event event;
  memset(&event, 0, sizeof(event));
  event.data.ptr = interest;
  event.events = makeEvent(mode);

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
  TRACE("findPrecedingWatcher for $0", interest);
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

LinuxScheduler::Watcher* LinuxScheduler::linkWatcher(Watcher* w, Watcher* pred) {
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

  return w;
}

LinuxScheduler::Watcher* LinuxScheduler::unlinkWatcher(Watcher* w) {
  Watcher* pred = w->prev;
  Watcher* succ = w->next;

  if (pred)
    pred->next = succ;
  else
    firstWatcher_ = succ;

  if (succ)
    succ->prev = pred;
  else
    lastWatcher_ = pred;

  w->clear();
  unref();

  return succ;
}

Duration LinuxScheduler::nextTimeout() const {
  if (!tasks_.empty())
    return Duration::Zero;

  const Duration a = !timers_.empty()
                 ? timers_.front()->when - now()
                 : 60_seconds;

  const Duration b = firstWatcher_ != nullptr
                 ? firstWatcher_->timeout - now()
                 : 61_seconds;

  return std::min(a, b);
}

void LinuxScheduler::cancelFD(int fd) { // TODO
}

void LinuxScheduler::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  wakeup->onWakeup(generation, std::bind(&LinuxScheduler::execute, this, task));
}

void LinuxScheduler::runLoop() {
  breakLoopCounter_.store(0);

  TRACE("runLoop: referenceCount=$0", referenceCount());
  while (referenceCount() > 0 && breakLoopCounter_.load() == 0) {
    runLoopOnce();
  }
}

void LinuxScheduler::runLoopOnce() {
  int rv;

  do rv = epoll_wait(epollfd_, &activeEvents_[0], activeEvents_.size(),
                     nextTimeout().milliseconds());
  while (rv < 0 && errno == EINTR);

  TRACE("runLoopOnce: epoll_wait returned $0", rv);

  if (rv < 0)
    RAISE_ERRNO(errno);

  std::list<Task> activeTasks;
  {
    std::lock_guard<std::mutex> lk(lock_);

    unref(tasks_.size());
    activeTasks = std::move(tasks_);

    collectActiveHandles(static_cast<size_t>(rv), &activeTasks);
    collectTimeouts(&activeTasks);
  }

  safeCall(onPreInvokePending_);
  safeCallEach(activeTasks);
  safeCall(onPostInvokePending_);
}

void LinuxScheduler::breakLoop() {
  TRACE("breakLoop()");
  breakLoopCounter_++;
  wakeupLoop();
}

EventLoop::HandleRef LinuxScheduler::insertIntoTimersList(MonotonicTime dt,
                                                          Task task) {
  TRACE("insertIntoTimersList() $0", dt);
  RefPtr<Timer> t(new Timer(dt, task));

  t->setCancelHandler([this, t]() {
    std::lock_guard<std::mutex> _l(lock_);

    auto i = timers_.end();
    auto e = timers_.begin();

    while (i != e) {
      i--;
      if (i->get() == t.get()) {
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
    const RefPtr<Timer>& current = *i;
    if (t->when >= current->when) {
      i++;
      i = timers_.insert(i, t);
      return t.as<Handle>();
    }
  }

  timers_.push_front(t);
  i = timers_.begin();
  return t.as<Handle>();

  // return std::make_shared<Handle>([this, i]() {
  //   std::lock_guard<std::mutex> lk(lock_);
  //   timers_.erase(i);
  // });
}

void LinuxScheduler::collectActiveHandles(size_t count,
                                          std::list<Task>* result) {
  for (size_t i = 0; i < count; ++i) {
    epoll_event& event = activeEvents_[i];
    if (Watcher* w = static_cast<Watcher*>(event.data.ptr)) {
      if (event.events & EPOLLIN) {
        readerCount_--;
        result->push_back(w->onIO);
        unlinkWatcher(w);
      } else if (event.events & EPOLLOUT) {
        writerCount_--;
        result->push_back(w->onIO);
        unlinkWatcher(w);
      }
    } else {
      // event.data.ptr is NULL, ie. that's our eventfd notification.
      uint64_t counter = 0;
      ::read(eventfd_, &counter, sizeof(counter));
    }
  }
}

void LinuxScheduler::collectTimeouts(std::list<Task>* result) {
  const auto nextTimedout = [this]() -> Watcher* {
    return firstWatcher_ && firstWatcher_->timeout <= now()
        ? firstWatcher_
        : nullptr;
  };

  while (Watcher* w = nextTimedout()) {
    result->push_back([w] { w->fire(w->onTimeout); });
    switch (w->mode) {
      case Mode::READABLE: readerCount_--; break;
      case Mode::WRITABLE: writerCount_--; break;
    }
    w = unlinkWatcher(w);
  }

  const auto nextTimer = [this]() -> RefPtr<Timer> {
    return !timers_.empty() && timers_.front()->when <= now()
        ? timers_.front()
        : nullptr;
  };

  while (RefPtr<Timer> job = nextTimer()) {
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

template<>
std::string StringUtil::toString<>(LinuxScheduler::Mode mode) {
  switch (mode) {
    case LinuxScheduler::Mode::READABLE:
      return "READABLE";
    case LinuxScheduler::Mode::WRITABLE:
      return "WRITABLE";
    default:
      return StringUtil::format("Mode<$0>", int(mode));
  }
}

template<>
std::string StringUtil::toString<>(LinuxScheduler::Watcher* w) {
  if (w != nullptr)
    return StringUtil::format("{fd: $0/$1, timeout: $2}",
                              w->fd, w->mode, w->timeout);
  else
    return "null";
}

} // namespace xzero
