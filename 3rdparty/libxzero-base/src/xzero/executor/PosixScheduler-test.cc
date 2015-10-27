// This file is part of the "libcortex" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libcortex is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/executor/PosixScheduler.h>
#include <xzero/MonotonicTime.h>
#include <xzero/MonotonicClock.h>
#include <xzero/application.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <memory>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <unistd.h>

using namespace xzero;

class SystemPipe { // {{{
 public:
  SystemPipe(int reader, int writer) {
    fds_[0] = reader;
    fds_[1] = writer;
  }

  SystemPipe() : SystemPipe(-1, -1) {
    if (pipe(fds_) < 0) {
      perror("pipe");
    }
  }

  ~SystemPipe() {
    closeEndPoint(0);
    closeEndPoint(1);
  }

  bool isValid() const noexcept { return fds_[0] != -1; }
  int readerFd() const noexcept { return fds_[0]; }
  int writerFd() const noexcept { return fds_[1]; }

  int write(const std::string& msg) {
    return ::write(writerFd(), msg.data(), msg.size());
  }

 private:
  inline void closeEndPoint(int i) {
    if (fds_[i] != -1) {
      ::close(fds_[i]);
    }
  }

 private:
  int fds_[2];
}; // }}}

/* test case:
 * 1.) insert interest A with timeout 10s
 * 2.) after 5 seconds, insert interest B with timeout 2
 * 3.) the interest B should now be fired after 2 seconds
 * 4.) the interest A should now be fired after 3 seconds
 */
TEST(PosixSchedulerTest, timeoutBreak) {
  PosixScheduler scheduler;
  SystemPipe a;
  SystemPipe b;
  MonotonicTime start = MonotonicClock::now();
  MonotonicTime a_fired_at;
  MonotonicTime b_fired_at;
  MonotonicTime a_timeout_at;
  MonotonicTime b_timeout_at;
  auto a_fired = [&]() { a_fired_at = MonotonicClock::now();
                         logTrace("x", "a_fired_at: $0", a_fired_at); };
  auto b_fired = [&]() { b_fired_at = MonotonicClock::now();
                         logTrace("x", "b_fired_at: $0", b_fired_at); };
  auto a_timeout = [&]() { a_timeout_at = MonotonicClock::now();
                           logTrace("x", "a_timeout_at: $0", a_timeout_at - start); };
  auto b_timeout = [&]() { b_timeout_at = MonotonicClock::now();
                           logTrace("x", "b_timeout_at: $0", b_timeout_at - start); };

  scheduler.executeOnReadable(a.readerFd(), a_fired,
                              Duration::fromMilliseconds(500), a_timeout);
  scheduler.executeOnReadable(b.readerFd(), b_fired,
                              Duration::fromMilliseconds(100), b_timeout);

  scheduler.runLoop();

  EXPECT_TRUE(!a_fired_at);
  EXPECT_TRUE(!b_fired_at);
  EXPECT_NEAR(500, (a_timeout_at - start).milliseconds(), 50);
  EXPECT_NEAR(100,  (b_timeout_at - start).milliseconds(), 50);
}

TEST(PosixSchedulerTest, executeAfter_without_handle) {
  PosixScheduler scheduler;
  MonotonicTime start;
  MonotonicTime firedAt;
  int fireCount = 0;

  scheduler.executeAfter(Duration::fromMilliseconds(50), [&](){
    firedAt = MonotonicClock::now();
    fireCount++;
  });

  start = MonotonicClock::now();
  firedAt = start;

  scheduler.runLoopOnce();

  Duration diff = firedAt - start;

  EXPECT_EQ(1, fireCount);
  EXPECT_NEAR(50, diff.milliseconds(), 10);
}

TEST(PosixSchedulerTest, executeAfter_cancel_beforeRun) {
  PosixScheduler scheduler;
  int fireCount = 0;

  auto handle = scheduler.executeAfter(Duration::fromSeconds(1), [&](){
    printf("****** cancel_beforeRun: running action\n");
    fireCount++;
  });

  EXPECT_EQ(1, scheduler.timerCount());
  handle->cancel();
  EXPECT_EQ(0, scheduler.timerCount());
  EXPECT_EQ(0, fireCount);
}

TEST(PosixSchedulerTest, executeAfter_cancel_beforeRun2) {
  PosixScheduler scheduler;
  int fire1Count = 0;
  int fire2Count = 0;

  auto handle1 = scheduler.executeAfter(Duration::fromSeconds(1), [&](){
    fire1Count++;
  });

  auto handle2 = scheduler.executeAfter(Duration::fromMilliseconds(10), [&](){
    fire2Count++;
  });

  EXPECT_EQ(2, scheduler.timerCount());
  handle1->cancel();
  EXPECT_EQ(1, scheduler.timerCount());

  scheduler.runLoopOnce();

  EXPECT_EQ(0, fire1Count);
  EXPECT_EQ(1, fire2Count);
}

TEST(PosixSchedulerTest, executeOnReadable) {
  // executeOnReadable: test cancellation after fire
  // executeOnReadable: test fire
  // executeOnReadable: test timeout
  // executeOnReadable: test fire at the time of the timeout

  PosixScheduler sched;

  SystemPipe pipe;
  int fireCount = 0;
  int timeoutCount = 0;

  pipe.write("blurb");

  auto handle = sched.executeOnReadable(
      pipe.readerFd(),
      [&] { fireCount++; },
      Duration::Zero,
      [&] { timeoutCount++; } );

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(0, timeoutCount);

  sched.runLoopOnce();

  EXPECT_EQ(1, fireCount);
  EXPECT_EQ(0, timeoutCount);
}

TEST(PosixSchedulerTest, executeOnReadable_timeout) {
  PosixScheduler sched;
  SystemPipe pipe;

  int fireCount = 0;
  int timeoutCount = 0;
  auto onFire = [&] { fireCount++; };
  auto onTimeout = [&] { timeoutCount++; };

  sched.executeOnReadable(pipe.readerFd(), onFire, Duration::fromMilliseconds(500), onTimeout);
  sched.runLoopOnce();

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(1, timeoutCount);
}

TEST(PosixSchedulerTest, executeOnReadable_timeout_on_cancelled) {
  PosixScheduler sched;
  SystemPipe pipe;

  int fireCount = 0;
  int timeoutCount = 0;
  auto onFire = [&] { fireCount++; };
  auto onTimeout = [&] {
    printf("onTimeout!\n");
    timeoutCount++; };

  auto handle = sched.executeOnReadable(
      pipe.readerFd(), onFire, Duration::fromMilliseconds(500), onTimeout);

  handle->cancel();
  sched.runLoopOnce();

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(0, timeoutCount);
}

// class AlreadyWatchingOnResource : public RuntimeError {
// public:
//   AlreadyWatchingOnResource()
//       : RuntimeError("Already watching on resource") {}
// };
// 
TEST(PosixSchedulerTest, executeOnReadable_twice_on_same_fd) {
  PosixScheduler sched;
  SystemPipe pipe;

  sched.executeOnReadable(pipe.readerFd(), [] () {});

  // FIXME
  // EXPECT_EXCEPTION("Already watching on resource", [&]() {
  //   sched.executeOnReadable(pipe.readerFd(), [] () {});
  // });

  // FIXME
  // EXPECT_THROW_STATUS(AlreadyWatchingOnResource,
  //     [&]() { sched.executeOnReadable(pipe.readerFd(), [] () {}); });
}

TEST(PosixSchedulerTest, executeOnWritable) {
  PosixScheduler sched;

  SystemPipe pipe;
  int fireCount = 0;
  int timeoutCount = 0;
  const Duration timeout = Duration::fromSeconds(1);
  const auto onFire = [&]() { fireCount++; };
  const auto onTimeout = [&]() { timeoutCount++; };

  sched.executeOnWritable(pipe.writerFd(), onFire, timeout, onTimeout);

  EXPECT_EQ(0, fireCount);
  EXPECT_EQ(0, timeoutCount);

  sched.runLoopOnce();

  EXPECT_EQ(1, fireCount);
  EXPECT_EQ(0, timeoutCount);
}

// TEST(PosixSchedulerTest, waitForReadable, [] () { // TODO
// });
// 
// TEST(PosixSchedulerTest, waitForWritable, [] () { // TODO
// });
// 
// TEST(PosixSchedulerTest, waitForReadable_timed, [] () { // TODO
// });
// 
// TEST(PosixSchedulerTest, waitForWritable_timed, [] () { // TODO
// });
