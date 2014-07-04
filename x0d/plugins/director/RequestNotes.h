// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/CustomDataMgr.h>
#include <x0/DateTime.h>
#include <x0/Buffer.h>
#include <x0/TokenShaper.h>
#include <x0/TimeSpan.h>
#include <x0/sysconfig.h>
#include <cstring>  // strlen()
#include <string>
#include <list>

#include "ClientAbortAction.h"

namespace x0 {
class HttpRequest;
class Source;
}

class Backend;
class BackendManager;

/**
 * Additional request attributes when using the director cluster.
 *
 * @see Director
 */
struct RequestNotes : public x0::CustomData {
  x0::HttpRequest* request;  //!< The actual HTTP request.
  x0::DateTime ctime;        //!< Request creation time.
  BackendManager* manager;   //!< Designated cluster to load balance this
                             //request.
  Backend* backend;          //!< Designated backend to serve this request.
  size_t tryCount;           //!< Number of request schedule attempts.
  ClientAbortAction onClientAbort;

  x0::TokenShaper<RequestNotes>::Node* bucket;  //!< the bucket (node) this
                                                //request is to be scheduled
                                                //via.
  size_t tokens;  //!< contains the number of currently acquired tokens by this
                  //request (usually 0 or 1).

#if defined(ENABLE_DIRECTOR_CACHE)
  std::string cacheKey;
  x0::TimeSpan cacheTTL;
  std::list<std::string> cacheHeaderIgnores;
  bool cacheIgnore;  //!< true if cache MUST NOT be preferred over the backend
                     //server's successful response.
#endif

  explicit RequestNotes(x0::HttpRequest* r);
  ~RequestNotes();

  void inspect(x0::Buffer& out);

#if defined(ENABLE_DIRECTOR_CACHE)
  void setCacheKey(const char* data, const char* eptr);
  void setCacheKey(const x0::BufferRef& fmt) {
    setCacheKey(fmt.data(), fmt.data() + fmt.size());
  }
#endif
};
