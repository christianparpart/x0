// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <string>

namespace xzero {
namespace http {
namespace client {

/**
 * Action/behavior how to react on client-side aborts.
 */
enum class ClientAbortAction {
  /**
   * Ignores the client abort.
   * That is, the upstream server will not notice that the client did abort.
   */
  Ignore = 0,

  /**
   * Close both endpoints.
   *
   * That is, closes connection to the upstream server as well as finalizes
   * closing the client connection.
   */
  Close = 1,

  /**
   * Notifies upstream.
   *
   * That is, the upstream server will be gracefully notified.
   * For FastCGI an \c AbortRequest message will be sent to upstream.
   * For HTTP this will cause the connection to the upstream server
   * to be closed (same as \c Close action).
   */
  Notify = 2,
};

// Try<ClientAbortAction> parseClientAbortAction(const std::string& val);
// 
// std::string tos(ClientAbortAction value);
// }}}

} // namespace client
} // namespace http
} // namespace xzero
