// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpListener.h>
#include <xzero/executor/Executor.h>
#include <xzero/CustomDataMgr.h>
#include <xzero/TokenShaper.h>
#include <memory>

namespace xzero::http::client {
  class HttpClient;
}

namespace xzero::http::cluster {

class Backend;

class Context : public CustomData,
                public HttpListener {
 public:
  Context() = delete;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  Context(const HttpRequest& _request,
          std::unique_ptr<HttpListener> _responseListener,
          Executor* _executor,
          size_t responseBodyBufferSize,
          const std::string& proxyId);
  ~Context();

  void post(Executor::Task task) { executor->execute(task); }

  // HttpListener overrides
  void onMessageBegin(HttpVersion version, HttpStatus code,
                      const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onError(std::error_code ec) override;

  // void onFailure(Context* cr, Status status);

 public:
  MonotonicTime ctime;
  Executor* executor;

  // the bucket (node) this request is to be scheduled via
  TokenShaper<Context>::Node* bucket;

  // designated backend to serve this request
  Backend* backend;
  std::unique_ptr<::xzero::http::client::HttpClient> client;

  // number of scheduling attempts
  size_t tryCount;

  // contains the number of currently acquired tokens by this request
  size_t tokens;

  HttpRequest request;

  HttpVersion proxyVersion_;
  std::string proxyId_;
  Buffer viaText_;

 private:
  std::unique_ptr<HttpListener> responseListener;
};

} // namespace xzero::http::cluster
