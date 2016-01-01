// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HttpChannel.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/sysconfig.h>
#include <list>
#include <string>

namespace xzero {
namespace http {
namespace http1 {

class Connection;

class Channel : public HttpChannel {
 public:
  Channel(Connection* transport,
          Executor* executor,
          const HttpHandler& handler,
          size_t maxRequestUriLength,
          size_t maxRequestBodyLength,
          HttpDateGenerator* dateGenerator,
          HttpOutputCompressor* outputCompressor);
  ~Channel();

  bool isPersistent() const noexcept { return persistent_; }
  void setPersistent(bool value) noexcept { persistent_ = value; }

  void reset() override;

  size_t bytesReceived() const noexcept;

 protected:
  void onMessageBegin(const BufferRef& method, const BufferRef& entity,
                      HttpVersion version) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onProtocolError(HttpStatus code, const std::string& message) override;

  void h2c_switching_protocols(const std::string& settings,
                               const HttpHandler& nextHandler);
  void h2c_start(const std::string& settings,
                 const HttpHandler& nextHandler);

 private:
  bool persistent_;
  HeaderFieldList connectionHeaders_;
  std::list<std::string> connectionOptions_;
};

} // namespace http1
} // namespace http
} // namespace xzero
