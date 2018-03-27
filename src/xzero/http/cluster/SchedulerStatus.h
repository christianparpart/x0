// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero::http::cluster {

/*!
 * Reflects the result of a request scheduling attempt.
 */
enum class SchedulerStatus {
  //! Request not scheduled, as all backends are offline and/or disabled.
  Unavailable,

  //! Request scheduled, Backend accepted request.
  Success,

  //!< Request not scheduled, as all backends available but overloaded or offline/disabled.
  Overloaded
};

} // namespace xzero::http::cluster
