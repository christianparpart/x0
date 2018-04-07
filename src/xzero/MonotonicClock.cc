// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/MonotonicClock.h>
#include <xzero/Duration.h>
#include <xzero/RuntimeError.h>
#include <xzero/defines.h>
#include <xzero/sysconfig.h>
#include <chrono>

#if defined(XZERO_OS_DARWIN)
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

namespace xzero {

#if defined(XZERO_OS_DARWIN)
mach_timebase_info_data_t timebaseInfo;

XZERO_INIT static void tbi_initialize() {
  mach_timebase_info(&timebaseInfo);
}
#endif

#if defined(XZERO_OS_WIN32)
// struct timespec { long tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1
int clock_gettime(int, struct timespec* ts)
{
  std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
  using namespace std::chrono;
  ts->tv_sec = duration_cast<seconds>(now.time_since_epoch()).count();
  ts->tv_nsec = duration_cast<nanoseconds>(now.time_since_epoch()).count() % 1000000000;

  return 0;
}
#endif

MonotonicTime MonotonicClock::now() {
#if defined(XZERO_OS_DARWIN)
  uint64_t machTimeUnits = mach_absolute_time();
  uint64_t nanos = machTimeUnits * timebaseInfo.numer / timebaseInfo.denom;

  return MonotonicTime(nanos);
#else
  // FIXME: doesn't work inside docker ubuntu:12.04
  // => defined(HAVE_CLOCK_GETTIME) // POSIX realtime API

  timespec ts;
  int rv = clock_gettime(CLOCK_MONOTONIC, &ts);

  if (rv < 0)
    RAISE_ERRNO(errno);

  uint64_t nanos =
      Duration::fromSeconds(ts.tv_sec).microseconds() * 1000 +
      ts.tv_nsec;

  return MonotonicTime(nanos);
#endif
}

} // namespace xzero
