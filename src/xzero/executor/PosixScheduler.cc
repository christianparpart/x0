// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/ExceptionHandler.h>
#include <xzero/MonotonicClock.h>
#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>
#include <xzero/WallClock.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/logging.h>
#include <xzero/net/Socket.h>
#include <xzero/sysconfig.h>
#include <xzero/thread/Wakeup.h>

#include <algorithm>
#include <vector>
#include <sstream>

#include <sys/types.h>
#include <fcntl.h>

#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif

#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

namespace xzero {

PosixScheduler::PosixScheduler(ExceptionHandler eh)
    : EventLoop{std::move(eh)},
      lock_{},
      wakeupPipe_{SocketPair::NonBlocking},
      tasks_{},
      timers_{},
      watchers_{},
      firstWatcher_{nullptr},
      lastWatcher_{nullptr},
      readerCount_{0},
      writerCount_{0},
      breakLoopCounter_{0},
      now_{MonotonicClock::now()},
      input_{},
      output_{},
      error_{} {
#if defined(HAVE_GETRLIMIT)
  struct rlimit rlim{};
  memset(&rlim, 0, sizeof(rlim));
  getrlimit(RLIMIT_NOFILE, &rlim);

  rlim_t nofile = rlim.rlim_max != RLIM_INFINITY
                ? rlim.rlim_max
                : 65535;

  watchers_.resize(nofile);
#else
  watchers_.resize(65535);
#endif
  for (auto& w: watchers_) {
    w = std::make_shared<Watcher>();
  }
}

PosixScheduler::PosixScheduler()
    : PosixScheduler(CatchAndLogExceptionHandler("PosixScheduler")) {
}

PosixScheduler::~PosixScheduler() {
}

void PosixScheduler::updateTime() {
  now_.update();
}

MonotonicTime PosixScheduler::now() const noexcept {
  return now_;
}

void PosixScheduler::execute(Task task) {
  {
    std::lock_guard<std::mutex> lk(lock_);
    tasks_.emplace_back(std::move(task));
    ref();
  }
  wakeupLoop();
}

std::string PosixScheduler::toString() const {
  return "PosixScheduler{}";
}

EventLoop::HandleRef PosixScheduler::executeAfter(Duration delay, Task task) {
  return insertIntoTimersList(now() + delay, task);
}

EventLoop::HandleRef PosixScheduler::executeAt(UnixTime when, Task task) {
  return insertIntoTimersList(now() + (when - WallClock::now()), task);
}

EventLoop::HandleRef PosixScheduler::insertIntoTimersList(MonotonicTime dt,
                                                          Task task) {
  auto t = std::make_shared<Timer>(dt, task);

  auto onCancel = [this, t]() {
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
  };
  t->setCancelHandler(onCancel);

  std::lock_guard<std::mutex> lk(lock_);

  ref();

  auto i = timers_.end();
  auto e = timers_.begin();

  while (i != e) {
    i--;
    const std::shared_ptr<Timer>& current = *i;
    if (t->when >= current->when) {
      i++;
      i = timers_.insert(i, t);
      return t;
    }
  }

  timers_.push_front(t);
  i = timers_.begin();
  return t;

  // return std::make_shared<Handle>([this, i]() {
  //   std::lock_guard<std::mutex> lk(lock_);
  //   timers_.erase(i);
  // });
}

void PosixScheduler::collectTimeouts(std::list<Task>* result) {
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
    unlinkWatcher(w);
  }

  const auto nextTimer = [this]() -> std::shared_ptr<Timer> {
    return !timers_.empty() && timers_.front()->when <= now()
        ? timers_.front()
        : nullptr;
  };

  while (std::shared_ptr<Timer> job = nextTimer()) {
    result->push_back([job] { job->fire(job->action); });
    timers_.pop_front();
    unref();
  }
}

PosixScheduler::HandleRef PosixScheduler::linkWatcher(Watcher* w, Watcher* pred) {
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

PosixScheduler::Watcher* PosixScheduler::unlinkWatcher(Watcher* w) {
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

Executor::HandleRef PosixScheduler::findWatcher(int fd) {
  std::lock_guard<std::mutex> lk(lock_);

  if (static_cast<size_t>(fd) < watchers_.size()) {
    return watchers_[fd];
  } else {
    return nullptr;
  }
}

EventLoop::HandleRef PosixScheduler::executeOnReadable(const Socket& s, Task task, Duration tmo, Task tcb) {
  readerCount_++;
  std::lock_guard<std::mutex> lk(lock_);
  return setupWatcher(s, Mode::READABLE, task, tmo, tcb);
}

EventLoop::HandleRef PosixScheduler::executeOnWritable(const Socket& s, Task task, Duration tmo, Task tcb) {
  writerCount_++;
  std::lock_guard<std::mutex> lk(lock_);
  return setupWatcher(s, Mode::WRITABLE, task, tmo, tcb);
}

PosixScheduler::HandleRef PosixScheduler::setupWatcher(
    int fd, Mode mode, Task task,
    Duration tmo, Task tcb) {

  MonotonicTime timeout = now() + tmo;

  if (static_cast<size_t>(fd) >= watchers_.size()) {
    // we cannot dynamically resize here without also updating the doubly linked
    // list as a realloc() can potentially change memory locations.
    logFatal("fd number too high");
  }

  Watcher* interest = watchers_[fd].get();

  if (interest->fd >= 0) {
    logFatal("PosixScheduler: Already watching on resource");
    //RAISE(AlreadyWatchingOnResource, "Already watching on resource");
    //RAISE_STATUS(AlreadyWatchingOnResource);
  }

  interest->reset(fd, mode, std::move(task), timeout, std::move(tcb));

  auto onCancel = [this, interest] () {
    std::lock_guard<std::mutex> _l(lock_);
    unlinkWatcher(interest);
  };

  interest->setCancelHandler(onCancel);

  if (!firstWatcher_)
    return linkWatcher(interest, nullptr);

  // inject watcher ordered by timeout ascending
  Watcher* succ = lastWatcher_;

  while (succ->prev != nullptr) {
    if (succ->prev->timeout < interest->timeout) {
      return linkWatcher(interest, succ->prev);
    } else {
      succ = succ->prev;
    }
  }

  if (firstWatcher_->timeout < interest->timeout) {
    // put in back
    return linkWatcher(interest, lastWatcher_);
  } else {
    // put in front
    return linkWatcher(interest, nullptr);
  }
}

void PosixScheduler::collectActiveHandles(std::list<Task>* result) {
  if (FD_ISSET(wakeupPipe_.left(), &input_))
    wakeupPipe_.left().consume();

  Watcher* w = firstWatcher_;

  while (w != nullptr) {
    if (FD_ISSET(w->fd, &input_)) {
      readerCount_--;
      result->push_back(w->onIO);
      w = unlinkWatcher(w);
    }
    else if (FD_ISSET(w->fd, &output_)) {
      writerCount_--;
      result->push_back(w->onIO);
      w = unlinkWatcher(w);
    } else {
      w = w->next;
    }
  }
}

// FIXME: this is actually so generic, it could be put into Executor API directly
void PosixScheduler::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  wakeup->onWakeup(generation, std::bind(&PosixScheduler::execute, this, task));
}

void PosixScheduler::runLoop() {
  loop(true);
}

void PosixScheduler::runLoopOnce() {
  loop(false);
}

void PosixScheduler::loop(bool repeat) {
  if (referenceCount() == 0)
    return;

  breakLoopCounter_.store(0);

  do {
    waitForEvents();
    std::list<Task> activeTasks = collectEvents();
    safeCallEach(activeTasks);
  } while (breakLoopCounter_.load() == 0 && repeat && referenceCount() > 0);
}

static std::string wsaErrorMessage(int err) {
  char buf[1024];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // flags
                nullptr,            // lpsource
                err,                // message id
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // languageid
                buf,                // output buffer
                sizeof(buf),        // size of msgbuf, bytes
                nullptr);           // va_list of arguments

  if (*buf)
    return buf;
  else
    return std::to_string(err);
}

size_t PosixScheduler::waitForEvents() noexcept {
  FD_ZERO(&input_);
  FD_ZERO(&output_);
  FD_ZERO(&error_);

  int wmark = collectWatches();

  FD_SET(wakeupPipe_.left().native(), &input_);
#if !defined(XZERO_OS_WINDOWS)
  wmark = std::max(wmark, wakeupPipe_.left().native());
#endif

  now_.update();

  // Computes the timespan the event loop should wait the most.
  constexpr Duration MaxLoopTimeout = 10_seconds;
  const Duration timeout =
      !tasks_.empty()
          ? Duration::Zero
          : std::min(!timers_.empty()
                          ? distance(now(), timers_.front()->when)
                          : MaxLoopTimeout,
                     firstWatcher_ != nullptr
                          ? distance(now(), firstWatcher_->timeout)
                          : MaxLoopTimeout + 1_seconds);

  int rv;
  timeval tv = timeout;
  do rv = ::select(wmark + 1, &input_, &output_, &error_, &tv);
  while (rv < 0 && errno == EINTR);
  // XXX select() may have updated the timeout (tv) to reflect how much
  // time is still left until the timeout would have been hit
  // Hence, (timeout - tv) equals the time waited

#if defined(XZERO_OS_WINDOWS)
  if (rv < 0)
    logFatal("select() returned unexpected error code: {}", wsaErrorMessage(WSAGetLastError()));
#else
  if (rv < 0)
    logFatal("select() returned unexpected error code: {}", strerror(errno));
#endif

  now_ = now_ + (timeout - Duration{tv});

  return rv;
}

std::list<EventLoop::Task> PosixScheduler::collectEvents() {
  std::lock_guard<std::mutex> lk(lock_);
  std::list<Task> activeTasks;

  unref(tasks_.size());
  activeTasks = std::move(tasks_);

  collectActiveHandles(&activeTasks);
  collectTimeouts(&activeTasks);

  return activeTasks;
}

int PosixScheduler::collectWatches() {
  std::lock_guard<std::mutex> lk(lock_);
  int wmark = 0;

  for (const Watcher* w = firstWatcher_; w; w = w->next) {
    if (w->fd < 0)
      continue;

    switch (w->mode) {
      case Mode::READABLE:
        FD_SET(w->fd, &input_);
        break;
      case Mode::WRITABLE:
        FD_SET(w->fd, &output_);
        break;
    }
    wmark = std::max(wmark, w->fd);
  }

  return wmark;
}

void PosixScheduler::breakLoop() {
  breakLoopCounter_++;
  wakeupLoop();
}

void PosixScheduler::wakeupLoop() {
  static const int dummy = 42;
  wakeupPipe_.right().write(&dummy, sizeof(dummy));
}

void PosixScheduler::waitForReadable(const Socket& s, Duration timeout) {
  fd_set input, output;

  FD_ZERO(&input);
  FD_ZERO(&output);
  FD_SET(s, &input);

  struct timeval tv = timeout;

  int res = select(s + 1, &input, &output, &input, &tv);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

void PosixScheduler::waitForReadable(const Socket& s) {
  fd_set input, output;

  FD_ZERO(&input);
  FD_ZERO(&output);
  FD_SET(s, &input);

  int res = select(s + 1, &input, &output, &input, nullptr);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

void PosixScheduler::waitForWritable(const Socket& s, Duration timeout) {
  fd_set input;
  FD_ZERO(&input);

  fd_set output;
  FD_ZERO(&output);
  FD_SET(s, &output);

  struct timeval tv = timeout;

  int res = select(s + 1, &input, &output, &input, &tv);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

void PosixScheduler::waitForWritable(const Socket& s) {
  fd_set input;
  FD_ZERO(&input);

  fd_set output;
  FD_ZERO(&output);
  FD_SET(s, &output);

  int res = select(s + 1, &input, &output, &input, nullptr);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

} // namespace xzero

