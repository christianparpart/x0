// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/PosixScheduler.h>
#include <xzero/MonotonicClock.h>
#include <xzero/thread/Wakeup.h>
#include <xzero/RuntimeError.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/ExceptionHandler.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

#include <algorithm>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>

namespace xzero {

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

#define ERROR(msg...) logError("PosixScheduler: " msg)

#if 0 // !defined(NDEBUG)
#define TRACE(msg...) logTrace("PosixScheduler", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

std::ostream& operator<<(std::ostream& os, PosixScheduler::Mode mode) {
  return os << inspect(mode);
}

std::ostream& operator<<(std::ostream& os, const PosixScheduler::Watcher& w) {
  return os << inspect(w);
}

std::ostream& operator<<(std::ostream& os, const PosixScheduler::Watcher* w) {
  if (w)
    return os << inspect(*w);
  else
    return os << "NULL";
}

std::ostream& operator<<(std::ostream& os, const PosixScheduler& s) {
  return os << inspect(s);
}

std::string inspect(PosixScheduler::Mode mode) {
  static const std::string modes[] = {
    "READABLE",
    "WRITABLE"
  };
  return modes[static_cast<size_t>(mode)];
}

std::string inspect(const std::list<std::shared_ptr<PosixScheduler::Timer>>& list) {
  std::string result;
  MonotonicTime now = MonotonicClock::now();
  result += "{";
  for (const auto& t: list) {
    if (result.size() > 1)
      result += ", ";
    result += StringUtil::format("{$0}", t->when - now);
  }
  result += "}";

  return result;
}

std::string inspect(const PosixScheduler::Timer& t) {
  return StringUtil::format("{$0}", t.when - MonotonicClock::now());
}

std::string inspect(const PosixScheduler::Watcher& w) {
  Duration diff = w.timeout - MonotonicClock::now();
  return StringUtil::format("{$0, $1, $2}",
                            w.fd, w.mode, diff);
}

std::string inspect(const PosixScheduler& s) {
  return s.inspectImpl();
}

PosixScheduler::PosixScheduler(
    ExceptionHandler eh,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : EventLoop(eh),
      lock_(),
      wakeupPipe_(),
      onPreInvokePending_(preInvoke),
      onPostInvokePending_(postInvoke),
      tasks_(),
      timers_(),
      watchers_(),
      firstWatcher_(nullptr),
      lastWatcher_(nullptr),
      readerCount_(0),
      writerCount_(0),
      breakLoopCounter_(0) {
  if (pipe(wakeupPipe_) < 0) {
    RAISE_ERRNO(errno);
  }
  fcntl(wakeupPipe_[0], F_SETFL, O_NONBLOCK);
  fcntl(wakeupPipe_[1], F_SETFL, O_NONBLOCK);

  struct rlimit rlim;
  memset(&rlim, 0, sizeof(rlim));
  getrlimit(RLIMIT_NOFILE, &rlim);

  rlim_t nofile = rlim.rlim_max != RLIM_INFINITY
                ? rlim.rlim_max
                : 65535;

  watchers_.resize(nofile);
  for (auto& w: watchers_)
    w = std::make_shared<Watcher>();

  TRACE("ctor: wakeupPipe {read=$0, write=$1, nofile=$2}",
      wakeupPipe_[PIPE_READ_END],
      wakeupPipe_[PIPE_WRITE_END],
      nofile);
}

PosixScheduler::PosixScheduler(ExceptionHandler eh)
    : PosixScheduler(eh, nullptr, nullptr) {
}

PosixScheduler::PosixScheduler()
    : PosixScheduler(CatchAndLogExceptionHandler("PosixScheduler")) {
}

PosixScheduler::~PosixScheduler() {
  TRACE("~dtor");
  ::close(wakeupPipe_[PIPE_READ_END]);
  ::close(wakeupPipe_[PIPE_WRITE_END]);
}

void PosixScheduler::updateTime() {
  now_ = MonotonicClock::now();
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
  return StringUtil::format("PosixScheduler: wakeupPipe{$0, $1}",
      wakeupPipe_[PIPE_READ_END],
      wakeupPipe_[PIPE_WRITE_END]);
}

EventLoop::HandleRef PosixScheduler::executeAfter(Duration delay, Task task) {
  TRACE("executeAfter: $0", delay);
  return insertIntoTimersList(now() + delay, task);
}

EventLoop::HandleRef PosixScheduler::executeAt(UnixTime when, Task task) {
  TRACE("executeAt: $0", when);
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
      TRACE("insertIntoTimersList: test if $0 >= $1 (yes, insert before)",
            inspect(*current), inspect(*t));
      i++;
      i = timers_.insert(i, t);
      return t;
    }
    TRACE("insertIntoTimersList: test if $0 >= $1 (no)",
          inspect(*current), inspect(*t));
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
  TRACE("collectTimeouts: consider $0 callbacks", timers_.size());
  const auto nextTimedout = [this]() -> Watcher* {
    return firstWatcher_ && firstWatcher_->timeout <= now()
        ? firstWatcher_
        : nullptr;
  };

  while (Watcher* w = nextTimedout()) {
    TRACE("collectTimeouts: timing out $0", *w);
    result->push_back([w] { w->fire(w->onTimeout); });
    switch (w->mode) {
      case Mode::READABLE: readerCount_--; break;
      case Mode::WRITABLE: writerCount_--; break;
    }
    w = unlinkWatcher(w);
  }

  const auto nextTimer = [this]() -> std::shared_ptr<Timer> {
    return !timers_.empty() && timers_.front()->when <= now()
        ? timers_.front()
        : nullptr;
  };

  while (std::shared_ptr<Timer> job = nextTimer()) {
    TRACE("collect next timer");
    result->push_back([job] { job->fire(job->action); });
    timers_.pop_front();
    unref();
  }
}

PosixScheduler::HandleRef PosixScheduler::linkWatcher(Watcher* w, Watcher* pred) {
  Watcher* succ = pred ? pred->next : firstWatcher_;

  w->prev = pred;
  w->next = succ;

  TRACE("linkWatcher $0 between $1 and $2", w, pred, succ);

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

EventLoop::HandleRef PosixScheduler::executeOnReadable(int fd, Task task, Duration tmo, Task tcb) {
  readerCount_++;
  std::lock_guard<std::mutex> lk(lock_);
  return setupWatcher(fd, Mode::READABLE, task, tmo, tcb);
}

EventLoop::HandleRef PosixScheduler::executeOnWritable(int fd, Task task, Duration tmo, Task tcb) {
  writerCount_++;
  std::lock_guard<std::mutex> lk(lock_);
  return setupWatcher(fd, Mode::WRITABLE, task, tmo, tcb);
}

void PosixScheduler::cancelFD(int fd) {
  HandleRef ref = findWatcher(fd);
  if (ref != nullptr) {
    ref->cancel();
  }
}

std::string inspectWatchers(PosixScheduler::Watcher* firstWatcher) {
  std::stringstream sstr;
  PosixScheduler::Watcher* prev = nullptr;
  PosixScheduler::Watcher* curr = firstWatcher;

  while (curr != nullptr) {
    MonotonicTime a;
    MonotonicTime b;
    if (prev) {
      a = curr->timeout;
      b = prev->timeout;
    } else {
      a = MonotonicClock::now();
      b = curr->timeout;
    }

    if (prev)
      sstr << ", ";

    sstr << StringUtil::format("{$0, $1, $2$3s}",
                               curr->fd,
                               curr->mode,
                               a <= b ? "+" : "-",
                               (a - b).seconds());

    prev = curr;
    curr = curr->next;
  }

  return sstr.str();
}

PosixScheduler::HandleRef PosixScheduler::setupWatcher(
    int fd, Mode mode, Task task,
    Duration tmo, Task tcb) {

  TRACE("setupWatcher($0, $1, $2)", fd, mode, tmo);
  TRACE("- $0", inspect(*this));

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

  interest->reset(fd, mode, task, timeout, tcb);

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
  if (FD_ISSET(wakeupPipe_[PIPE_READ_END], &input_)) {
    bool consumeMore = true;
    while (consumeMore) {
      char buf[sizeof(int) * 128];
      consumeMore = ::read(wakeupPipe_[PIPE_READ_END], buf, sizeof(buf)) > 0;
    }
  }

  Watcher* w = firstWatcher_;

  while (w != nullptr) {
    if (FD_ISSET(w->fd, &input_)) {
      TRACE("collectActiveHandles: + active fd $0 READABLE", w->fd);
      readerCount_--;
      result->push_back(w->onIO);
      w = unlinkWatcher(w);
    }
    else if (FD_ISSET(w->fd, &output_)) {
      TRACE("collectActiveHandles: + active fd $0 WRITABLE", w->fd);
      writerCount_--;
      result->push_back(w->onIO);
      w = unlinkWatcher(w);
    } else {
      TRACE("collectActiveHandles: - skip fd $0", w->fd);
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

void PosixScheduler::logLoopStats(const char* prefix) {
#if !defined(NDEBUG)
  std::lock_guard<std::mutex> _l(lock_);
  size_t watcherCount = 0;

  for (Watcher* w = firstWatcher_; w != nullptr; w = w->next)
    watcherCount++;

  TRACE("$0: tasks=$1, timers=$2, watchers=$3, refs=$4, breakLoop=$5",
      prefix, tasks_.size(), timers_.size(), watcherCount, referenceCount(),
      breakLoopCounter_.load());
#endif
}

void PosixScheduler::loop(bool repeat) {
  if (referenceCount() == 0)
    return;

  breakLoopCounter_.store(0);
  updateTime();

  do {
    logLoopStats("loop()");
    waitForEvents();
    std::list<Task> activeTasks = collectEvents();
    safeCall(onPreInvokePending_);
    safeCallEach(activeTasks);
    safeCall(onPostInvokePending_);
    if (!activeTasks.empty()) {
      updateTime();
    }
  } while (breakLoopCounter_.load() == 0 && repeat && referenceCount() > 0);

  logLoopStats("loop(at exit)");
}

size_t PosixScheduler::waitForEvents() noexcept {
  FD_ZERO(&input_);
  FD_ZERO(&output_);
  FD_ZERO(&error_);

  timeval tv;
  int incount = 0;
  int outcount = 0;
  int errcount = 0;
  int wmark = 0;

  FD_SET(wakeupPipe_[PIPE_READ_END], &input_);
  wmark = std::max(wmark, wakeupPipe_[PIPE_READ_END]);

  collectWatches(&incount, &outcount, &wmark);

  Duration nt = nextTimeout();
  TRACE("waitForEvents: nextTimeout = $0", nt);
  tv = nt;

  TRACE("waitForEvents: select(wmark=$0, in=$1, out=$2, err=$3, tmo=$4), timers=$5, tasks=$6",
        wmark + 1, incount, outcount, errcount, Duration(tv), timers_.size(), tasks_.size());
  TRACE("waitForEvents: $0", inspect(*this).c_str());

  int rv;
  do rv = ::select(wmark + 1, &input_, &output_, &error_, &tv);
  while (rv < 0 && errno == EINTR);

  if (rv < 0)
    logFatal("select() returned unexpected error code: $0", strerror(errno));

  updateTime();

  TRACE("waitForEvents: select returned $0", rv);
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

void PosixScheduler::collectWatches(int* incount, int* outcount, int* wmark) {
  std::lock_guard<std::mutex> lk(lock_);

  for (const Watcher* w = firstWatcher_; w; w = w->next) {
    if (w->fd < 0)
      continue;

    switch (w->mode) {
      case Mode::READABLE:
        FD_SET(w->fd, &input_);
        (*incount)++;
        break;
      case Mode::WRITABLE:
        FD_SET(w->fd, &output_);
        (*outcount)++;
        break;
    }
    *wmark = std::max(*wmark, w->fd);
  }
}

Duration PosixScheduler::nextTimeout() const {
  if (!tasks_.empty())
    return Duration::Zero;

  TRACE("nextTimeout: timers = $0", inspect(timers_));

  const Duration a = !timers_.empty()
                 ? timers_.front()->when - now()
                 : 60_seconds;

  const Duration b = firstWatcher_ != nullptr
                 ? firstWatcher_->timeout - now()
                 : 61_seconds;

  return std::min(a, b);
}

void PosixScheduler::breakLoop() {
  TRACE("breakLoop()");
  breakLoopCounter_++;
  wakeupLoop();
}

void PosixScheduler::wakeupLoop() {
  int dummy = 42;
  ::write(wakeupPipe_[PIPE_WRITE_END], &dummy, sizeof(dummy));
}

void PosixScheduler::waitForReadable(int fd, Duration timeout) {
  fd_set input, output;

  FD_ZERO(&input);
  FD_ZERO(&output);
  FD_SET(fd, &input);

  struct timeval tv = timeout;

  int res = select(fd + 1, &input, &output, &input, &tv);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

void PosixScheduler::waitForReadable(int fd) {
  fd_set input, output;

  FD_ZERO(&input);
  FD_ZERO(&output);
  FD_SET(fd, &input);

  int res = select(fd + 1, &input, &output, &input, nullptr);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

void PosixScheduler::waitForWritable(int fd, Duration timeout) {
  fd_set input;
  FD_ZERO(&input);

  fd_set output;
  FD_ZERO(&output);
  FD_SET(fd, &output);

  struct timeval tv = timeout;

  int res = select(fd + 1, &input, &output, &input, &tv);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

void PosixScheduler::waitForWritable(int fd) {
  fd_set input;
  FD_ZERO(&input);

  fd_set output;
  FD_ZERO(&output);
  FD_SET(fd, &output);

  int res = select(fd + 1, &input, &output, &input, nullptr);

  if (res == 0) {
    throw TimeoutError{"unexpected timeout while select()ing"};
  }

  if (res == -1) {
    RAISE_ERRNO(errno);
  }
}

std::string PosixScheduler::inspectImpl() const {
  std::stringstream sstr;

  sstr << "{";
  sstr << "wakeupPipe:" << wakeupPipe_[PIPE_READ_END]
       << "/" << wakeupPipe_[PIPE_WRITE_END];

  sstr << ", watchers(" << inspectWatchers(firstWatcher_) << ")";
  sstr << ", front:" << (firstWatcher_ ? firstWatcher_->fd : -1);
  sstr << ", back:" << (lastWatcher_ ? lastWatcher_->fd : -1);
  sstr << "}"; // scheduler

  return sstr.str();
}

} // namespace xzero
