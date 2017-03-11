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
#include <vector>

#include <sys/epoll.h>
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
  MonotonicTime time = now() + delay;
  TRACE("executeAfter: $0", time);
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
    RAISE(AlreadyWatchingOnResource, "Already watching on resource");
  }

  Watcher* interest = new Watcher(fd, mode, task, now() + timeout, tcb);
  watchers_[fd] = RefPtr<Watcher>(interest);

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
    return wi->second.as<Handle>();
  } else {
    return nullptr;
  }
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

  TRACE("runLoop: referenceCount=$0", referenceCount());
  while (referenceCount() > 0 && breakLoopCounter_.load() == 0) {
    runLoopOnce();
  }
}

void LinuxScheduler::runLoopOnce() {
  int rv;

  uint64_t timeoutMillis = nextTimeout().milliseconds();

  do rv = epoll_wait(epollfd_, &activeEvents_[0], activeEvents_.size(),
                     timeoutMillis);
  while (rv < 0 && errno == EINTR);

  TRACE("runLoopOnce: epoll_wait(fd=$0, count=$1, timeout=$2) = $3 $4",
        epollfd_.get(), activeEvents_.size(), timeoutMillis, rv,
        rv < 0 ? strerror(rv) : "");

  if (rv < 0) {
    RAISE_ERRNO(errno);
  } else if (rv == 0 && timeoutMillis > 0) {
    usleep(Duration::fromMilliseconds(timeoutMillis).microseconds());
  }

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
  RefPtr<Timer> tref(new Timer(dt, task));
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
    const RefPtr<Timer>& current = *i;
    if (t->when >= current->when) {
      i++;
      i = timers_.insert(i, tref);
      return tref.as<Handle>();
    }
  }

  timers_.push_front(tref);
  i = timers_.begin();
  return tref.as<Handle>();

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
        TRACE("collectActiveHandles: $0 READABLE", w->fd);
        readerCount_--;
        w->ref();
        result->push_back([w] { w->fire(w->onIO); w->unref(); });
        unlinkWatcher(w);
      } else if (event.events & EPOLLOUT) {
        TRACE("collectActiveHandles: $0 WRITABLE", w->fd);
        writerCount_--;
        w->ref();
        result->push_back([w] { w->fire(w->onIO); w->unref(); });
        unlinkWatcher(w);
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
  const auto nextTimedout = [this]() -> Watcher* {
    return firstWatcher_ && firstWatcher_->timeout <= now()
        ? firstWatcher_
        : nullptr;
  };

  while (Watcher* w = nextTimedout()) {
    switch (w->mode) {
      case Mode::READABLE: readerCount_--; break;
      case Mode::WRITABLE: writerCount_--; break;
    }
    w->ref();
    result->push_back([w] { w->fire(w->onTimeout); w->unref(); });
    w = unlinkWatcher(w);
  }

  const auto nextTimer = [this]() -> Timer* {
    return !timers_.empty() && timers_.front()->when <= now()
        ? timers_.front().get()
        : nullptr;
  };

  while (Timer* job = nextTimer()) {
    TRACE("collectTimeouts: job $0", job->when);
    job->ref();
    result->push_back([job] { job->fire(job->action); job->unref(); });
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
