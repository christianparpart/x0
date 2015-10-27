#include <xzero/MonotonicClock.h>
#include <xzero/Duration.h>
#include <xzero/RuntimeError.h>
#include <xzero/Defines.h>
#include <xzero/sysconfig.h>

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

MonotonicTime MonotonicClock::now() {
#if defined(HAVE_CLOCK_GETTIME) // POSIX realtime API
  timespec ts;
  int rv = clock_gettime(CLOCK_MONOTONIC, &ts);

  if (rv < 0)
    RAISE_ERRNO(errno);

  uint64_t nanos =
      Duration::fromSeconds(ts.tv_sec).microseconds() * 1000 +
      ts.tv_nsec;

  return MonotonicTime(nanos);
#elif defined(XZERO_OS_DARWIN)
  uint64_t machTimeUnits = mach_absolute_time();
  uint64_t nanos = machTimeUnits * timebaseInfo.numer / timebaseInfo.denom;

  return MonotonicTime(nanos);
#else
  // TODO
  #error "MonotonicClock: Your platform is not implemented."
#endif
}

} // namespace xzero
