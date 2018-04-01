// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <fmt/format.h>

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

namespace fmt {
  template<>
  struct formatter<xzero::http::cluster::SchedulerStatus> {
    using SchedulerStatus = xzero::http::cluster::SchedulerStatus;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const SchedulerStatus& v, FormatContext &ctx) {
      switch (v) {
        case SchedulerStatus::Unavailable:
          return format_to(ctx.begin(), "Unavailable");
        case SchedulerStatus::Success:
          return format_to(ctx.begin(), "Success");
        case SchedulerStatus::Overloaded:
          return format_to(ctx.begin(), "Overloaded");
        default:
          return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}

