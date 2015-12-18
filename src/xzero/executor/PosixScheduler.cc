// This file is part of the "libcortex" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libcortex is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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

#define ERROR(msg...) logError("PosixScheduler", msg)

#if 0 // !defined(NDEBUG)
#define TRACE(msg...) logTrace("PosixScheduler", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

template<>
std::string StringUtil::toString<PosixScheduler::Mode>(PosixScheduler::Mode mode) {
  return inspect(mode);
}

template<>
std::string StringUtil::toString<PosixScheduler::Watcher>(PosixScheduler::Watcher w) {
  return inspect(w);
}

template<>
std::string StringUtil::toString<PosixScheduler::Watcher*>(PosixScheduler::Watcher* w) {
  if (!w)
    return "nil";

  return inspect(*w);
}

template<>
std::string StringUtil::toString<const PosixScheduler&>(const PosixScheduler& s) {
  return inspect(s);
}

std::string inspect(PosixScheduler::Mode mode) {
  static const std::string modes[] = {
    "READABLE",
    "WRITABLE"
  };
  return modes[static_cast<size_t>(mode)];
}

std::string inspect(const std::list<RefPtr<PosixScheduler::Timer>>& list) {
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
    std::unique_ptr<xzero::ExceptionHandler> eh,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : Scheduler(std::move(eh)),
      lock_(),
      wakeupPipe_(),
      onPreInvokePending_(preInvoke),
      onPostInvokePending_(postInvoke),
      tasks_(),
      timers_(),
      watchers_(),
      firstWatcher_(nullptr),
      lastWatcher_(nullptr) {
  if (pipe(wakeupPipe_) < 0) {
    RAISE_ERRNO(errno);
  }
  fcntl(wakeupPipe_[0], F_SETFL, O_NONBLOCK);
  fcntl(wakeupPipe_[1], F_SETFL, O_NONBLOCK);

  struct rlimit rlim;
  memset(&rlim, 0, sizeof(rlim));
  getrlimit(RLIMIT_NOFILE, &rlim);
  size_t nofile = rlim.rlim_max;

  watchers_.resize(nofile);

  TRACE("ctor: wakeupPipe {read=$0, write=$1, nofile=$2}",
      wakeupPipe_[PIPE_READ_END],
      wakeupPipe_[PIPE_WRITE_END],
      nofile);
}

PosixScheduler::PosixScheduler(std::unique_ptr<xzero::ExceptionHandler> eh)
    : PosixScheduler(std::move(eh), nullptr, nullptr) {
}

PosixScheduler::PosixScheduler()
    : PosixScheduler(std::unique_ptr<ExceptionHandler>(new CatchAndLogExceptionHandler("PosixScheduler"))) {
}

PosixScheduler::~PosixScheduler() {
  TRACE("~dtor");
  ::close(wakeupPipe_[PIPE_READ_END]);
  ::close(wakeupPipe_[PIPE_WRITE_END]);
}

MonotonicTime PosixScheduler::now() const {
  // later to provide cachable value
  return MonotonicClock::now();
}

void PosixScheduler::execute(Task task) {
  {
    std::lock_guard<std::mutex> lk(lock_);
    tasks_.emplace_back(std::move(task));
  }
  breakLoop();
}

std::string PosixScheduler::toString() const {
  return StringUtil::format("PosixScheduler: wakeupPipe{$0, $1}",
      wakeupPipe_[PIPE_READ_END],
      wakeupPipe_[PIPE_WRITE_END]);
}

Scheduler::HandleRef PosixScheduler::executeAfter(Duration delay, Task task) {
  TRACE("executeAfter: $0", delay);
  return insertIntoTimersList(now() + delay, task);
}

Scheduler::HandleRef PosixScheduler::executeAt(UnixTime when, Task task) {
  TRACE("executeAt: $0", when);
  return insertIntoTimersList(now() + (when - WallClock::now()), task);
}

Scheduler::HandleRef PosixScheduler::insertIntoTimersList(MonotonicTime dt,
                                                          Task task) {
  RefPtr<Timer> t(new Timer(dt, task));

  auto onCancel = [this, t]() {
    auto i = timers_.end();
    auto e = timers_.begin();

    while (i != e) {
      i--;
      if (i->get() == t.get()) {
        timers_.erase(i);
        break;
      }
    }
  };
  t->setCancelHandler(onCancel);

  std::lock_guard<std::mutex> lk(lock_);

  auto i = timers_.end();
  auto e = timers_.begin();

  while (i != e) {
    i--;
    const RefPtr<Timer>& current = *i;
    if (t->when >= current->when) {
      TRACE("insertIntoTimersList: test if $0 >= $1 (yes, insert before)",
            inspect(*current), inspect(*t));
      i++;
      i = timers_.insert(i, t);
      return t.as<Handle>();
    }
    TRACE("insertIntoTimersList: test if $0 >= $1 (no)",
          inspect(*current), inspect(*t));
  }

  timers_.push_front(t);
  i = timers_.begin();
  return t.as<Handle>();

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

  const auto nextTimer = [this]() -> RefPtr<Timer> {
    return !timers_.empty() && timers_.front()->when <= now()
        ? timers_.front()
        : nullptr;
  };

  while (RefPtr<Timer> job = nextTimer()) {
    TRACE("collect next timer");
    result->push_back([job] { job->fire(job->action); });
    timers_.pop_front();
  }
}

void PosixScheduler::linkWatcher(Watcher* w, Watcher* pred) {
  Watcher* succ = pred->next;

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

  return succ;
}

Scheduler::HandleRef PosixScheduler::executeOnReadable(int fd, Task task, Duration tmo, Task tcb) {
  readerCount_++;
  std::lock_guard<std::mutex> lk(lock_);
  return setupWatcher(fd, Mode::READABLE, task, tmo, tcb);
}

Scheduler::HandleRef PosixScheduler::executeOnWritable(int fd, Task task, Duration tmo, Task tcb) {
  writerCount_++;
  std::lock_guard<std::mutex> lk(lock_);
  return setupWatcher(fd, Mode::WRITABLE, task, tmo, tcb);
}

void PosixScheduler::cancelFD(int fd) {
  std::lock_guard<std::mutex> lk(lock_);
  if (fd < watchers_.size()) {
    Watcher* w = &watchers_[fd];
    w->cancel();
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

  if (fd >= watchers_.size()) {
    // we cannot dynamically resize here without also updating the doubly linked
    // list as a realloc() can potentially change memory locations.
    RAISE(IOError, "fd number too high");
  }

  Watcher* interest = &watchers_[fd];

  if (interest->fd >= 0)
    RAISE(AlreadyWatchingOnResource, "Already watching on resource");
    //RAISE_STATUS(AlreadyWatchingOnResource);

  interest->reset(fd, mode, task, timeout, tcb);

  // inject watcher ordered by timeout ascending
  if (lastWatcher_ != nullptr) {
    Watcher* succ = lastWatcher_;

    while (succ->prev != nullptr) {
      if (succ->prev->timeout < interest->timeout) {
        linkWatcher(interest, succ->prev);
        breakLoop();
        return interest;
      } else {
        succ = succ->prev;
      }
    }

    if (firstWatcher_->timeout < interest->timeout) {
      // put in back
      interest->prev = lastWatcher_;
      lastWatcher_->next = interest;
      lastWatcher_ = interest;
    } else {
      // put in front
      interest->next = firstWatcher_;
      firstWatcher_->prev = interest;
      firstWatcher_ = interest;
    }
  } else {
    firstWatcher_ = lastWatcher_ = interest;
  }

  breakLoop();
  return interest; // handle;
}

void PosixScheduler::collectActiveHandles(const fd_set* input,
                                          const fd_set* output,
                                          std::list<Task>* result) {
  Watcher* w = firstWatcher_;

  while (w != nullptr) {
    if (FD_ISSET(w->fd, input)) {
      TRACE("collectActiveHandles: + active fd $0 READABLE", w->fd);
      readerCount_--;
      result->push_back(w->onIO);
      w = unlinkWatcher(w);
    }
    else if (FD_ISSET(w->fd, output)) {
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

size_t PosixScheduler::timerCount() {
  std::lock_guard<std::mutex> lk(lock_);
  return timers_.size();
}

size_t PosixScheduler::readerCount() {
  return readerCount_.load();
}

size_t PosixScheduler::writerCount() {
  return writerCount_.load();
}

size_t PosixScheduler::taskCount() {
  std::lock_guard<std::mutex> lk(lock_);
  return tasks_.size();
}

void PosixScheduler::runLoop() {
  for (;;) {
    lock_.lock();
    bool cont = !tasks_.empty()
             || !timers_.empty()
             || firstWatcher_ != nullptr;
    lock_.unlock();

    if (!cont)
      break;

    runLoopOnce();
  }
}

void PosixScheduler::runLoopOnce() {
  fd_set input, output, error;
  FD_ZERO(&input);
  FD_ZERO(&output);
  FD_ZERO(&error);

  int wmark = 0;
  timeval tv;
  int incount = 0;
  int outcount = 0;
  int errcount = 0;

  FD_SET(wakeupPipe_[PIPE_READ_END], &input);
  wmark = std::max(wmark, wakeupPipe_[PIPE_READ_END]);

  {
    std::lock_guard<std::mutex> lk(lock_);

    for (const Watcher* w = firstWatcher_; w; w = w->next) {
      if (w->fd < 0)
        continue;

      switch (w->mode) {
        case Mode::READABLE:
          FD_SET(w->fd, &input);
          incount++;
          break;
        case Mode::WRITABLE:
          FD_SET(w->fd, &output);
          outcount++;
          break;
      }
      wmark = std::max(wmark, w->fd);
    }

    Duration nt = nextTimeout();
    TRACE("runLoopOnce(): nextTimeout = $0", nt);
    tv = nt;
  }

  TRACE("runLoopOnce: select(wmark=$0, in=$1, out=$2, err=$3, tmo=$4), timers=$5, tasks=$6",
        wmark + 1, incount, outcount, errcount, Duration(tv), timers_.size(), tasks_.size());
  TRACE("runLoopOnce: $0", inspect(*this).c_str());

  int rv;
  do rv = ::select(wmark + 1, &input, &output, &error, &tv);
  while (rv < 0 && errno == EINTR);

  if (rv < 0)
    RAISE_ERRNO(errno);

  TRACE("runLoopOnce: select returned $0", rv);

  if (FD_ISSET(wakeupPipe_[PIPE_READ_END], &input)) {
    bool consumeMore = true;
    while (consumeMore) {
      char buf[sizeof(int) * 128];
      consumeMore = ::read(wakeupPipe_[PIPE_READ_END], buf, sizeof(buf)) > 0;
    }
  }

  std::list<Task> activeTasks;
  {
    std::lock_guard<std::mutex> lk(lock_);

    activeTasks = std::move(tasks_);
    collectActiveHandles(&input, &output, &activeTasks);
    collectTimeouts(&activeTasks);
  }

  safeCall(onPreInvokePending_);
  safeCallEach(activeTasks);
  safeCall(onPostInvokePending_);
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
    RAISE(IOError, "unexpected timeout while select()ing");
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
    RAISE(IOError, "unexpected timeout while select()ing");
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
    RAISE(IOError, "unexpected timeout while select()ing");
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
    RAISE(IOError, "unexpected timeout while select()ing");
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
