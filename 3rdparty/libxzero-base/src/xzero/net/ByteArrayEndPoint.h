// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <xzero/Buffer.h>
#include <xzero/net/EndPoint.h>

namespace xzero {

class LocalConnector;

/**
 * Buffer-based dual-channel EndPoint.
 *
 * @see LocalEndPoint
 * @see InetEndPoint
 */
class XZERO_BASE_API ByteArrayEndPoint : public EndPoint {
 public:
  explicit ByteArrayEndPoint(LocalConnector* connector);
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
  size_t fill(Buffer*) override;
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

 private:
  LocalConnector* connector_;
  Buffer input_;
  size_t readPos_;
  Buffer output_;
  bool closed_;
};

} // namespace xzero
