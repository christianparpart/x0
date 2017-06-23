// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
#include <xzero/Option.h>

namespace xzero {

/**
 * An informational data structure that is passed to signal handlers.
 *
 * @see UnixSignals
 */
struct UnixSignalInfo {
  int signal;      //!< signal number
  Option<int> pid; //!< sender's process-ID
  Option<int> uid; //!< sender's real user-ID
};

} // namespace xzero
