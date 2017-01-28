// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/fastcgi/bits.h>
#include <xzero/http/http1/Parser.h>
#include <xzero/Buffer.h>
#include <unordered_map>
#include <functional>
#include <utility>

namespace xzero {
namespace http {

class HttpListener;

namespace fastcgi {

/**
 * Parses a client FastCGI stream (upstream & downstream side).
 */
class XZERO_HTTP_API ResponseParser {
 private:
  struct StreamState { // {{{
    HttpListener* listener;
    size_t totalBytesReceived;
    bool contentFullyReceived;
    http::http1::Parser http1Parser;

    StreamState();
    ~StreamState();
    void reset();
    void setListener(HttpListener* listener);
  }; // }}}

 public:
  ResponseParser(
      std::function<HttpListener*(int requestId)> onCreateChannel,
      std::function<void(int requestId, int recordId)> onUnknownPacket,
      std::function<void(int requestId, const BufferRef&)> onStdErr);

  void reset();

  StreamState& registerStreamState(int requestId);
  void removeStreamState(int requestId);

  size_t parseFragment(const BufferRef& chunk);
  size_t parseFragment(const Record& record);

  template<typename RecordType, typename... Args>
  size_t parseFragment(Args... args) {
    return parseFragment(RecordType(args...));
  }

 protected:
  StreamState& getStream(int requestId);
  void process(const fastcgi::Record* record);
  void streamStdOut(const fastcgi::Record* record);
  void streamStdErr(const fastcgi::Record* record);

 protected:
  std::function<HttpListener*(int requestId)> onCreateChannel_;
  std::function<void(int requestId, int recordId)> onUnknownPacket_;
  std::function<void(int requestId)> onAbortRequest_;
  std::function<void(int requestId, const BufferRef& content)> onStdErr_;

  std::unordered_map<int, StreamState> streams_;
};

} // namespace fastcgi
} // namespace http
} // namespace xzero
