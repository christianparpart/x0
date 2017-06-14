// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/Api.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/RuntimeError.h>
#include <system_error>

namespace xzero {
namespace http {

/**
 * Helper exception that is thrown on semantic message errors by HttpChannel.
 */
class XZERO_HTTP_API BadMessage : public RuntimeError {
 public:
  explicit BadMessage(HttpStatus code);
  BadMessage(HttpStatus code, const std::string& reason);
  explicit BadMessage(RuntimeError& v) : RuntimeError(v) {}

  HttpStatus httpCode() const noexcept {
    return static_cast<HttpStatus>(code().value());
  }
};

#define RAISE_HTTP(status) RAISE_EXCEPTION(BadMessage, (HttpStatus:: status))
#define RAISE_HTTP_REASON(status, reason) RAISE_EXCEPTION(BadMessage, (HttpStatus:: status), reason)

} // namespace http
} // namespace xzero
