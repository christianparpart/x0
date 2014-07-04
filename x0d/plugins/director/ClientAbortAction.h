// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <x0/Api.h>
#include <x0/Buffer.h>
#include <x0/Try.h>

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

X0_API x0::Try<ClientAbortAction> parseClientAbortAction(
    const x0::BufferRef& value);
X0_API std::string tos(ClientAbortAction value);
