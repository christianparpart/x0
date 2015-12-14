#pragma once

#include <xzero/MonotonicTime.h>

namespace xzero {

/**
 * Monotonic Clock Provider API.
 *
 * This API is mainly usable in Scheduler implementations to
 * effectively implement timers and timeouts.
 */
class MonotonicClock {
public:
  /**
   * Retrieves the current monotonic time.
   */
  static MonotonicTime now();
};

} // namespace xzero
