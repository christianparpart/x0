// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpListener.h>
#include <xzero/executor/Executor.h>
#include <xzero/CustomDataMgr.h>
#include <xzero/TokenShaper.h>
#include <xzero/io/InputStream.h>
#include <memory>

namespace xzero {

class InputStream;

namespace http {
namespace client {

class HttpClusterMember;

struct HttpClusterRequest : public CustomData {
  HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                     std::unique_ptr<InputStream> _requestBody,
                     std::unique_ptr<HttpListener> _responseListener,
                     Executor* _executor);

  MonotonicTime ctime;
  const HttpRequestInfo& requestInfo;
  std::unique_ptr<InputStream> requestBody;
  std::unique_ptr<HttpListener> responseListener;
  Executor* executor;

  // the bucket (node) this request is to be scheduled via
  TokenShaper<HttpClusterRequest>::Node* bucket;

  // designated backend to serve this request
  HttpClusterMember* backend;

  // number of scheduling attempts
  size_t tryCount;

  // contains the number of currently acquired tokens by this request
  size_t tokens;

  void post(Executor::Task task) { executor->execute(task); }
};

} // namespace http
} // namespace client
} // namespace xzero
