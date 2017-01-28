// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/CustomDataMgr.h>
#include <base/UnixTime.h>
#include <base/Buffer.h>
#include <base/TokenShaper.h>
#include <base/Duration.h>
#include <x0d/sysconfig.h>
#include <cstring>  // strlen()
#include <string>
#include <list>

#include "ClientAbortAction.h"

namespace base {
class Source;
}

namespace xzero {
class HttpRequest;
}

class Backend;
class BackendManager;

/**
 * Additional request attributes when using the director cluster.
 *
 * @see Director
 */
struct RequestNotes : public base::CustomData {
  xzero::HttpRequest* request; //!< The actual HTTP request.
  base::UnixTime ctime;        //!< Request creation time.

  //! Designated cluster to load balance this request.
  BackendManager* manager;     

  Backend* backend;            //!< Designated backend to serve this request.
  size_t tryCount;             //!< Number of request schedule attempts.
  ClientAbortAction onClientAbort;

  //! the bucket (node) this request is to be scheduled via.
  base::TokenShaper<RequestNotes>::Node* bucket;

  /*! contains the number of currently acquired tokens by this request.
   *
   * (usually 0 or 1)
   */
  size_t tokens;

#if defined(ENABLE_DIRECTOR_CACHE)
  std::string cacheKey;
  base::Duration cacheTTL;
  std::list<std::string> cacheHeaderIgnores;
  bool cacheIgnore;  //!< true if cache MUST NOT be preferred over the backend
                     //server's successful response.
#endif

  explicit RequestNotes(xzero::HttpRequest* r);
  ~RequestNotes();

  void inspect(base::Buffer& out);

#if defined(ENABLE_DIRECTOR_CACHE)
  void setCacheKey(const char* data, const char* eptr);
  void setCacheKey(const base::BufferRef& fmt) {
    setCacheKey(fmt.data(), fmt.data() + fmt.size());
  }
#endif
};
