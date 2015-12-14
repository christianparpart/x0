// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

namespace xzero {
namespace http {
namespace client {

/*!
 * Reflects the result of a request scheduling attempt.
 */
enum class HttpClusterSchedulerStatus {
  //! Request not scheduled, as all backends are offline and/or disabled.
  Unavailable,

  //! Request scheduled, Backend accepted request.
  Success,

  //!< Request not scheduled, as all backends available but overloaded or offline/disabled.
  Overloaded
};

} // namespace http
} // namespace client
} // namespace xzero
