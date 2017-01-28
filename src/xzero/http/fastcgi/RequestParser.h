// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/fastcgi/bits.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/Buffer.h>
#include <unordered_map>
#include <functional>
#include <utility>
#include <string>
#include <list>

namespace xzero {
namespace http {

class HttpListener;

namespace fastcgi {

/**
 * Parses a client FastCGI request stream.
 *
 */
class XZERO_HTTP_API RequestParser {
 private:
  struct StreamState : public CgiParamStreamReader { // {{{
    HttpListener* listener;
    size_t totalBytesReceived;
    bool paramsFullyReceived;
    bool contentFullyReceived;
    HeaderFieldList params;
    HeaderFieldList headers;

    // keeps the body at least as long as params haven't been fully parsed.
    Buffer body;

    StreamState();
    ~StreamState();

    void reset();

    void onParam(const char *name, size_t nameLen,
                 const char *value, size_t valueLen) override;
  }; // }}}

 public:
  RequestParser(
      std::function<HttpListener*(int requestId, bool keepAlive)> onCreateChannel,
      std::function<void(int requestId, int recordId)> onUnknownPacket,
      std::function<void(int requestId)> onAbortRequest);

  void reset();

  StreamState* registerStreamState(int requestId);
  StreamState* getStream(int requestId);
  void removeStreamState(int requestId);

  size_t parseFragment(const BufferRef& chunk);
  size_t parseFragment(const Record& record);

  template<typename RecordType, typename... Args>
  size_t parseFragment(Args... args) {
    return parseFragment(RecordType(args...));
  }

 protected:
  void process(const fastcgi::Record* record);
  void beginRequest(const fastcgi::BeginRequestRecord* record);
  void streamParams(const fastcgi::Record* record);
  void streamStdIn(const fastcgi::Record* record);
  void streamStdOut(const fastcgi::Record* record);
  void streamStdErr(const fastcgi::Record* record);
  void streamData(const fastcgi::Record* record);
  void abortRequest(const fastcgi::AbortRequestRecord* record);

 protected:
  std::function<HttpListener*(int requestId, bool keepAlive)> onCreateChannel_;
  std::function<void(int requestId, int recordId)> onUnknownPacket_;
  std::function<void(int requestId)> onAbortRequest_;

  std::unordered_map<int, StreamState> streams_;
};

} // namespace fastcgi
} // namespace http
} // namespace xzero
