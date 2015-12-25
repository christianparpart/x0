#pragma once

namespace xzero {
namespace http {
namespace http2 {

class FrameListener {
 public:
  virtual ~FrameListener() {}

  virtual void onData(const BufferRef& data) = 0;
  virtual void onHeaders() = 0;
  virtual void onPriority() = 0;
  virtual void onPing() = 0;
  virtual void onGoAway() = 0;
  virtual void onReset() = 0;
  virtual void onSettings() = 0;
  virtual void onPushPromise() = 0;
  virtual void onWindowUpdate() = 0;

  virtual void onProtocolError(const std::string& errorMessage) = 0;
};

class Parser {
 public:
  explicit Parser(FrameListener* listener);

  bool decode(const BufferRef& chunk);

 protected:
  void data();
  void headers();
  void priority();
  void resetStream();

 private:
  FrameListener* listener_;
};

} // namespace http2
} // namespace http
} // namespace xzero
