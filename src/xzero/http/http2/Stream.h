// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/http2/StreamState.h>
#include <xzero/http/http2/StreamID.h>
#include <xzero/http/HttpTransport.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/io/DataChain.h>

namespace xzero {

class Executor;

namespace http {

class HttpDateGenerator;
class HttpOutputCompressor;

namespace http2 {

class Connection;
class Stream;

bool streamCompare(Stream* a, Stream* b);

// XXX PriorityTree<> ?
// typedef DependencyTree<Stream*, streamCompare> StreamTree;
// typedef StreamTree::Node StreamTreeNode;

class Stream : public ::xzero::http::HttpTransport {
 public:
  Stream(StreamID id,
         Connection* connection,
         Executor* executor,
         const HttpHandler& handler,
         size_t maxRequestUriLength,
         size_t maxRequestBodyLength,
         HttpDateGenerator* dateGenerator,
         HttpOutputCompressor* outputCompressor);

  unsigned id() const noexcept;
  StreamState state() const noexcept;
  HttpChannel* channel() const noexcept;

  void sendWindowUpdate(size_t windowSize);
  void appendBody(const BufferRef& data);
  void handleRequest();

 public:
  void setCompleter(CompletionHandler onComplete);

  void sendHeaders(const HttpResponseInfo& info);
  void close();

  // HttpTransport overrides
  void abort() override;
  void completed() override;
  void send(HttpResponseInfo& responseInfo, Buffer&& chunk,
            CompletionHandler onComplete) override;
  void send(HttpResponseInfo& responseInfo, const BufferRef& chunk,
            CompletionHandler onComplete) override;
  void send(HttpResponseInfo& responseInfo, FileView&& chunk,
            CompletionHandler onComplete) override;
  void send(Buffer&& chunk, CompletionHandler onComplete) override;
  void send(const BufferRef& chunk, CompletionHandler onComplete) override;
  void send(FileView&& chunk, CompletionHandler onComplete) override;

 private:
  Connection* connection_;                // HTTP/2 connection layer
  std::unique_ptr<HttpChannel> channel_;  // HTTP semantics layer
  StreamID id_;                           // stream id
  StreamState state_;                     // default: Idle
  int weight_;                            // default: 16
  //StreamTreeNode* node_;                  // ref in the stream dependency tree
  DataChain body_;                        // pending response body chunks
  CompletionHandler onComplete_;
};

inline bool streamCompare(Stream* a, Stream* b) {
  return a->id() == b->id();
}

} // namespace http2
} // namespace http
} // namespace xzero
