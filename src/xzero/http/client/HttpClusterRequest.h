// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/HttpRequestInfo.h>
#include <xzero/TokenShaper.h>
#include <xzero/io/InputStream.h>
#include <memory>

namespace xzero {

class InputStream;
class Executor;

namespace http {

class HttpListener;

namespace client {

class HttpClusterMember;

struct HttpClusterRequest {
  HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                     std::unique_ptr<InputStream> _requestBody,
                     HttpListener* _responseListener,
                     Executor* _executor);

  const HttpRequestInfo& requestInfo;
  std::unique_ptr<InputStream> requestBody;
  HttpListener* responseListener;
  Executor* executor;

  // the bucket (node) this request is to be scheduled via
  TokenShaper<HttpClusterRequest>::Node* bucket;

  // designated backend to serve this request
  HttpClusterMember* backend;

  // number of scheduling attempts
  size_t tryCount;

  // contains the number of currently acquired tokens by this request
  size_t tokens;
};

} // namespace http
} // namespace client
} // namespace xzero
