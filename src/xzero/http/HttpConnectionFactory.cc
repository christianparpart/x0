// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpConnectionFactory.h>
#include <xzero/WallClock.h>

namespace xzero {
namespace http {

HttpConnectionFactory::HttpConnectionFactory(
    const std::string& protocolName,
    size_t maxRequestUriLength,
    size_t maxRequestBodyLength)
    : protocolName_(protocolName),
      maxRequestUriLength_(maxRequestUriLength),
      maxRequestBodyLength_(maxRequestBodyLength),
      outputCompressor_(std::make_unique<HttpOutputCompressor>()),
      dateGenerator_() {
  //.
}

HttpConnectionFactory::~HttpConnectionFactory() {
  //.
}

void HttpConnectionFactory::setHandlerFactory(HttpHandlerFactory&& factory) {
  handlerFactory_ = std::move(factory);
}

}  // namespace http
}  // namespace xzero
