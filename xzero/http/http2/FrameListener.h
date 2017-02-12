// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/http2/StreamID.h>
#include <xzero/http/http2/ErrorCode.h>
#include <xzero/http/http2/SettingParameter.h>
#include <vector>
#include <list>
#include <utility>

namespace xzero {
namespace http {

class HttpRequestInfo;

namespace http2 {

class FrameListener {
 public:
  virtual ~FrameListener() {}

  // frame type callbacks
  virtual void onData(StreamID sid, const BufferRef& data, bool last) = 0;
  virtual void onRequestBegin(StreamID sid, bool noContent,
                              HttpRequestInfo&& info) = 0;

  virtual void onPriority(StreamID sid,
                          bool isExclusiveDependency,
                          StreamID streamDependency,
                          unsigned weight) = 0;

  virtual void onPing(const BufferRef& data) = 0;
  virtual void onPingAck(const BufferRef& data) = 0;
  virtual void onGoAway(StreamID sid, ErrorCode errorCode, const BufferRef& debugData) = 0;

  virtual void onResetStream(StreamID sid, ErrorCode errorCode) = 0;

  virtual void onSettings(const std::vector<std::pair<SettingParameter, unsigned long>>& settings) = 0;
  virtual void onSettingsAck() = 0;
  virtual void onPushPromise(StreamID sid, StreamID promisedStreamID,
                             HttpRequestInfo&& info) = 0;
  virtual void onWindowUpdate(StreamID sid, uint32_t increment) = 0;

  // error callbacks
  virtual void onConnectionError(ErrorCode ec, const std::string& message) = 0;
  virtual void onStreamError(StreamID sid, ErrorCode ec,
                             const std::string& message) = 0;
};

} // namespace http2
} // namespace http
} // namespace xzero
