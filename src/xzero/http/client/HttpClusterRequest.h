// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/client/HttpClient.h>
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

class HttpClusterRequest : public CustomData,
                            public HttpListener {
 public:
  HttpClusterRequest() = delete;
  HttpClusterRequest(const HttpClusterRequest&) = delete;
  HttpClusterRequest& operator=(const HttpClusterRequest&) = delete;

  HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                     const BufferRef& _requestBody,
                     std::unique_ptr<HttpListener> _responseListener,
                     Executor* _executor,
                     size_t responseBodyBufferSize,
                     const std::string& proxyId);
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

  void onFailure(HttpClusterRequest* cr, Status status);

 public:
  MonotonicTime ctime;
  HttpClient client;
  Executor* executor;

  // the bucket (node) this request is to be scheduled via
  TokenShaper<HttpClusterRequest>::Node* bucket;

  // designated backend to serve this request
  HttpClusterMember* backend;

  // number of scheduling attempts
  size_t tryCount;

  // contains the number of currently acquired tokens by this request
  size_t tokens;

  HttpVersion proxyVersion_;
  std::string proxyId_;
  Buffer viaText_;

 private:
  std::unique_ptr<HttpListener> responseListener;
};

} // namespace http
} // namespace client
} // namespace xzero
