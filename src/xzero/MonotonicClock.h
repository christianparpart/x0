// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
