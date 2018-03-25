// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "RequestNotes.h"
#include "Backend.h"

#include <xzero/HttpRequest.h>
#include <xzero/HttpConnection.h>
#include <xzero/HttpWorker.h>
#include <base/CustomDataMgr.h>
#include <base/TokenShaper.h>
#include <base/Duration.h>
#include <x0d/sysconfig.h>

using namespace base;
using namespace xzero;

RequestNotes::RequestNotes(HttpRequest* r)
    : request(r),
      ctime(r->connection.worker().now()),
      manager(nullptr),
      backend(nullptr),
      tryCount(0),
      onClientAbort(ClientAbortAction::Close),
      bucket(nullptr),
      tokens(0)
#if defined(ENABLE_DIRECTOR_CACHE)
      ,
      cacheKey(),
      cacheTTL(Duration::Zero),
      cacheHeaderIgnores(),
      cacheIgnore(false)
#endif
{
  r->registerInspectHandler<RequestNotes, &RequestNotes::inspect>(this);
}

RequestNotes::~RequestNotes() {
  if (bucket && tokens) {
    // XXX We should never reach here, because tokens must have been put back by
    // Director::release() already.
    bucket->put(tokens);
    tokens = 0;
  }
}

void RequestNotes::inspect(Buffer& out) {
  if (backend) {
    out.printf("backend: %s\n", backend->name().c_str());
  } else {
    out.printf("backend: null\n");
  }
}

#if defined(ENABLE_DIRECTOR_CACHE)  // {{{
void RequestNotes::setCacheKey(const char* i, const char* e) {
  Buffer result;

  while (i != e) {
    if (*i == '%') {
      ++i;
      if (i != e) {
        switch (*i) {
          case 's':  // scheme
            if (!request->connection.isSecure())
              result.push_back("http");
            else
              result.push_back("https");
            break;
          case 'h':  // host header
            result.push_back(request->requestHeader("Host"));
            break;
          case 'r':  // request path
            result.push_back(request->path);
            break;
          case 'q':  // query args
            result.push_back(request->query);
            break;
          case '%':
            result.push_back(*i);
            break;
          default:
            result.push_back('%');
            result.push_back(*i);
        }
        ++i;
      } else {
        result.push_back('%');
        break;
      }
    } else {
      result.push_back(*i);
      ++i;
    }
  }

  cacheKey = result.str();
}
#endif  // }}}
