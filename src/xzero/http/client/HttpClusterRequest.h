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
#include <memory>

namespace xzero {

class InputStream;

namespace http {

class HttpListener;

namespace client {

class HttpClusterRequest {
 public:
  HttpClusterRequest();

  HttpListener* responseListener() const { return responseListener_; }

 private:
  HttpRequestInfo requestInfo_;
  std::unique_ptr<InputStream> requestBody_;
  HttpListener* responseListener_;
  size_t tryCount_;

  // the bucket (node) this request is to be scheduled via.
  TokenShaper<HttpClusterRequest>::Node* bucket_;
};

} // namespace http
} // namespace client
} // namespace xzero
