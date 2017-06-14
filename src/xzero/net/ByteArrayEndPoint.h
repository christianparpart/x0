// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Buffer.h>
#include <xzero/net/EndPoint.h>

namespace xzero {

/**
 * Buffer-based dual-channel EndPoint.
 *
 * @see LocalEndPoint
 * @see InetEndPoint
 */
class XZERO_BASE_API ByteArrayEndPoint : public EndPoint {
 public:
  ByteArrayEndPoint();
  ~ByteArrayEndPoint();

  /**
   * Assigns an input to this endpoint.
   *
   * @param input a movable buffer object.
   */
  void setInput(Buffer&& input);

  /**
   * Assigns an input to this endpoint.
   *
   * @param input a C-string.
   */
  void setInput(const char* input);

  /**
   * Retrieves a reference to the input buffer.
   */
  const Buffer& input() const;

  /**
   * Retrieves a reference to the output buffer.
   */
  const Buffer& output() const;

  // overrides
  void close() override;
  bool isOpen() const override;
  std::string toString() const override;
  using EndPoint::fill;
  size_t fill(Buffer* sink, size_t count) override;
  size_t flush(const BufferRef&) override;
  size_t flush(int fd, off_t offset, size_t size) override;
  void wantFill() override;
  void wantFlush() override;
  Duration readTimeout() override;
  Duration writeTimeout() override;
  void setReadTimeout(Duration timeout) override;
  void setWriteTimeout(Duration timeout) override;
  bool isBlocking() const override;
  void setBlocking(bool enable) override;
  bool isCorking() const override;
  void setCorking(bool enable) override;
  bool isTcpNoDelay() const override;
  void setTcpNoDelay(bool enable) override;

 private:
  Buffer input_;
  size_t readPos_;
  Buffer output_;
  bool closed_;
};

} // namespace xzero
