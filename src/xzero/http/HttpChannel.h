// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Callback.h>
#include <xzero/CompletionHandler.h>
#include <xzero/http/HttpListener.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/io/Filter.h>
#include <list>
#include <memory>
#include <iosfwd>

namespace xzero {

class FileView;
class Executor;
class HugeBuffer;

namespace http {

class HttpResponseInfo;
class HttpTransport;
class HttpRequest;
class HttpResponse;
class HttpOutputCompressor;
class HttpDateGenerator;

enum class HttpChannelState {
  READING,  //!< currently reading request info
  HANDLING, //!< currently handling the request (that is generating response)
  SENDING,  //!< currently sending data
};

std::string as_string(HttpChannelState state);
std::ostream& operator<<(std::ostream& os, HttpChannelState state);

/**
 * Semantic HTTP message exchange layer.
 *
 * An HttpChannel implements the semantic layer ontop of the transport layer.
 *
 * @see HttpTransport
 */
class HttpChannel : public HttpListener {
 public:
  HttpChannel(HttpTransport* transport,
              Executor* executor,
              const HttpHandlerFactory& handlerFactory,
              size_t maxRequestUriLength,
              size_t maxRequestBodyLength,
              HttpDateGenerator* dateGenerator,
              HttpOutputCompressor* outputCompressor);
  ~HttpChannel();

  Executor* executor() const noexcept;

  /**
   * Resets the channel state.
   *
   * After invokation this channel is as it would have been just instanciated.
   */
  virtual void reset();

  /**
   * Sends a response body chunk @p data.
   *
   * @param data body chunk
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   *
   * The response will auto-commit the response status line and
   * response headers if not done yet.
   */
  void send(Buffer&& data, CompletionHandler onComplete);

  /**
   * Sends a response body chunk @p data.
   *
   * @param data body chunk
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   *
   * The response will auto-commit the response status line and
   * response headers if not done yet.
   *
   * @note You must ensure the data chunk is available until sending completed!
   */
  void send(const BufferRef& data, CompletionHandler onComplete);

  /**
   * Sends a response body chunk as defined by @p file.
   *
   * @param file the system file handle containing the data to be sent to the
   *             client.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   *
   * The response will auto-commit the response status line and
   * response headers if not done yet.
   */
  void send(FileView&& file, CompletionHandler onComplete);

  /**
   * Sends a response body chunk.
   *
   * @param content the content to be sent to the client.
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   *
   * The response will auto-commit the response status line and
   * response headers if not done yet.
   */
  void send(HugeBuffer&& content, CompletionHandler&& completed);

  /**
   * Sends an 100-continue intermediate response message.
   */
  void send100Continue(CompletionHandler onComplete);

  /**
   * Retrieves the request object for the current request.
   */
  HttpRequest* request() const { return request_.get(); }

  /**
   * Retrieves the response object for the current response.
   */
  HttpResponse* response() const { return response_.get(); }

  /**
   * Invoked by HttpResponse::completed() in to inform this channel about its
   * completion.
   */
  void completed();

  // HttpListener overrides
  void onMessageBegin(const BufferRef& method, const BufferRef& entity,
                      HttpVersion version) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onError(std::error_code ec) override;

  /**
   * Commits response headers.
   *
   * @param onComplete callback invoked when sending chunk is succeed/failed.
   */
  void commit(CompletionHandler onComplete);

  /**
   * Adds a custom output-filter.
   *
   * @param filter the output filter to apply to the output body.
   *
   * The filter will not take over ownership. Make sure the filter is
   * available for the whole time the response is generated.
   */
  void addOutputFilter(std::shared_ptr<Filter> filter);

  /**
   * Removes all output-filters.
   */
  void removeAllOutputFilters();

  HttpChannelState state() const noexcept { return state_; }
  void setState(HttpChannelState newState);

  // hooks
  void onPostProcess(std::function<void()> callback);
  void onResponseEnd(std::function<void()> callback);

  // event, only to be invoked by transport implementors
  void responseEnd(); // no, via cb functor instead

 protected:
  void handleRequest();
  void onBeforeSend();
  HttpResponseInfo& commitInline();

 protected:
  size_t maxRequestUriLength_;
  size_t maxRequestBodyLength_;
  HttpChannelState state_;
  HttpTransport* transport_;
  Executor* executor_;
  std::unique_ptr<HttpRequest> request_;
  std::unique_ptr<HttpResponse> response_;
  HttpDateGenerator* dateGenerator_;
  std::list<std::shared_ptr<Filter>> outputFilters_;
  HttpOutputCompressor* outputCompressor_;
  HttpHandlerFactory handlerFactory_;
  std::function<void()> handler_;

  Callback<void()> onPostProcess_;
  Callback<void()> onResponseEnd_;
};

inline Executor* HttpChannel::executor() const noexcept {
  return executor_;
}

std::ostream& operator<<(std::ostream& os, HttpChannel* value);

}  // namespace http
}  // namespace xzero
