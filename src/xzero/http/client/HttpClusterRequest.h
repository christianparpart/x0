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

struct HttpClusterRequest : public CustomData,
                            public HttpListener {
 public:
  HttpClusterRequest() = delete;
  HttpClusterRequest(const HttpClusterRequest&) = delete;
  HttpClusterRequest& operator=(const HttpClusterRequest&) = delete;

  HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                     const BufferRef& _requestBody,
                     std::unique_ptr<HttpListener> _responseListener,
                     Executor* _executor);
  ~HttpClusterRequest();

  void post(Executor::Task task) { executor->execute(task); }

  // HttpListener overrides
  void onMessageBegin(HttpVersion version, HttpStatus code,
                      const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onProtocolError(HttpStatus code, const std::string& message) override;

 public:
  MonotonicTime ctime;
  const HttpRequestInfo& requestInfo;
  BufferRef requestBody;
  Executor* executor;

  // the bucket (node) this request is to be scheduled via
  TokenShaper<HttpClusterRequest>::Node* bucket;

  // designated backend to serve this request
  HttpClusterMember* backend;

  // number of scheduling attempts
  size_t tryCount;

  // contains the number of currently acquired tokens by this request
  size_t tokens;

 private:
  std::unique_ptr<HttpListener> responseListener;
};

} // namespace http
} // namespace client
} // namespace xzero
