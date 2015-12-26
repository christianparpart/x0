// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/http2/SettingParameter.h>
#include <vector>
#include <utility>

namespace xzero {
namespace http {
namespace http2 {

class FrameListener {
 public:
  virtual ~FrameListener() {}

  // frame type callbacks
  virtual void onData(const BufferRef& data) = 0;
  virtual void onHeaders() = 0;
  virtual void onPriority() = 0;
  virtual void onPing() = 0;
  virtual void onGoAway() = 0;
  virtual void onReset() = 0;
  virtual void onSettings(const std::vector<std::pair<SettingParameter, unsigned long>>& settings) = 0;
  virtual void onPushPromise() = 0;
  virtual void onWindowUpdate() = 0;

  // connection error callbacks
  virtual void onProtocolError(const std::string& errorMessage) = 0;
  virtual void onFrameSizeError(const std::string& errorMessage) = 0;
};

} // namespace http2
} // namespace http
} // namespace xzero
