#pragma once
#include <xzero/http/HttpTransport.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/net/Connection.h>
#include <deque>
#include <cstdint>

namespace xzero {

class EndPoint;
class Executor;

namespace http {

class HttpDateGenerator;
class HttpOutputCompressor;

namespace http2 {

class Connection
  : public ::xzero::Connection,
    public ::xzero::http::HttpTransport {
 public:
  Connection(EndPoint* ep,
             Executor* executor,
             const HttpHandler& handler,
             HttpDateGenerator* dateGenerator,
             HttpOutputCompressor* outputCompressor,
             size_t maxRequestUriLength,
             size_t maxRequestBodyLength,
             size_t maxRequestCount);

  void setMaxConcurrentStreams(size_t value);
  size_t maxConcurrentStreams() const;

 private:
  size_t lowestStreamIdLocal_;
  size_t lowestStreamIdRemote_;
  size_t maxStreamIdLocal_;
  size_t maxStreamIdRemote_;
  std::deque<Stream> clientStreams_;
  std::deque<Stream> pushPromiseStreams_;

};

inline size_t Connection::maxConcurrentStreams() const {
  return streams_.size();
}

} // namespace http2
} // namespace http
} // namespace xzero
