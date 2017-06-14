// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Parser.h>
#include <xzero/http/http2/FrameListener.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::http2;

class NoopFrameListener : public http2::FrameListener {
 public:
  void onData(StreamID sid, const BufferRef& data, bool last) override {}
  void onRequestBegin(StreamID sid, bool noContent, HttpRequestInfo&& info) override {}
  void onPriority(StreamID sid, bool isExclusiveDependency, StreamID streamDependency, unsigned weight) override {}
  void onPing(const BufferRef& data) override {}
  void onPingAck(const BufferRef& data) override {}
  void onGoAway(StreamID sid, ErrorCode errorCode, const BufferRef& debugData) override {}
  void onResetStream(StreamID sid, ErrorCode errorCode) override {}
  void onSettings(const std::vector<std::pair<SettingParameter, unsigned long>>& settings) override {}
  void onSettingsAck() override {}
  void onPushPromise(StreamID sid, StreamID promisedStreamID, HttpRequestInfo&& info) override {}
  void onWindowUpdate(StreamID sid, uint32_t increment) override {}
  void onConnectionError(ErrorCode ec, const std::string& message) override {}
  void onStreamError(StreamID sid, ErrorCode ec, const std::string& message) override {}
};

TEST(http_http2_Parser, wip) {
  NoopFrameListener listener;
  http2::Parser parser(&listener);
  // TODO: next time
}
