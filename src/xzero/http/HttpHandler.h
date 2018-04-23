// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <functional>
#include <memory>

namespace xzero::http {

class HttpRequest;
class HttpResponse;

typedef std::function<void(HttpRequest*, HttpResponse*)> HttpHandler;

/**
 * Creates a new HTTP request handler to serve the given @p request.
 *
 * @param request  request object to serve
 * @param response resonse object used for the reply
 *
 * @returns An HttpHandler used to serve the given @p request.
 */
typedef std::function<std::function<void()>(HttpRequest*, HttpResponse*)> HttpHandlerFactory;

}  // namespace xzero::http
