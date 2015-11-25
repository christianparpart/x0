// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <string>

namespace xzero {
namespace http {

/**
 * Callback API for HTTP message body availability.
 */
class XZERO_HTTP_API HttpInputListener {
 public:
  virtual ~HttpInputListener() {}

  /**
   * Invoked on some content data being available.
   */
  virtual void onContentAvailable() = 0;

  /**
   * Invoked when all data has been fully consumed.
   *
   * No other callbacks will be invoked once this one got called.
   */
  virtual void onAllDataRead() = 0;

  /**
   * Invoked when an input read error occured.
   *
   * @param errorMessage a describing error message for logging.
   *
   * No other callbacks will be invoked once this one got called.
   */
  virtual void onError(const std::string& errorMessage) = 0;
};

} // namespace http
} // namespace xzero
