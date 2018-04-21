// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/WallClock.h>
#include <xzero/defines.h>

#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace xzero {

#if defined(XZERO_OS_WINDOWS)
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
  // XXX: https://stackoverflow.com/a/26085827/386670
  //
  // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
  // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
  // until 00:00:00 January 1, 1970 
  static const uint64_t EPOCH = (uint64_t) 116444736000000000ULL;

  SYSTEMTIME  system_time;
  FILETIME    file_time;
  uint64_t    time;

  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);
  time = ((uint64_t) file_time.dwLowDateTime);
  time += ((uint64_t) file_time.dwHighDateTime) << 32;

  tp->tv_sec = (long)((time - EPOCH) / 10000000L);
  tp->tv_usec = (long)(system_time.wMilliseconds * 1000);

  return 0;
}
#endif

UnixTime WallClock::now() {
  return UnixTime(WallClock::getUnixMicros());
}

uint64_t WallClock::unixSeconds() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return tv.tv_sec;
}

uint64_t WallClock::getUnixMillis() {
  return unixMillis();
}

uint64_t WallClock::unixMillis() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000llu + tv.tv_usec / 1000llu;
}

uint64_t WallClock::getUnixMicros() {
  return unixMicros();
}

uint64_t WallClock::unixMicros() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000llu + tv.tv_usec;
}

}
