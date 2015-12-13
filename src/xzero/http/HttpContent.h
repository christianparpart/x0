// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/Buffer.h>
#include <xzero/io/FileView.h>
#include <xzero/io/FileDescriptor.h>

namespace xzero {
namespace http {

/**
 * Represents an HTTP message body for use of HTTP request and response mssages.
 */
class HttpContent {
 public:
  class Builder;

  HttpContent();
  HttpContent(const HttpContent&) = delete;
  HttpContent& operator=(const HttpContent&) = delete;
  explicit HttpContent(const BufferRef& value);
  explicit HttpContent(Buffer&& value);
  HttpContent(FileDescriptor&& fd, size_t size);
  ~HttpContent();

  /**
   * Tests whether or not this HTTP content has no content.
   */
  bool empty() const noexcept;

  /**
   * Retrieves the HTTP content size is in bytes.
   */
  size_t size() const;

  /**
   * Tests whether the HTTP content is backed as a file.
   *
   * @retval true HTTP content is backed in a file.
   * @retval false HTTP content is backed in memory-only.
   */
  bool isFile() const { return fd_.isOpen(); }

  /**
   * Retrieves a FileView object that represents the HTTP content.
   */
  FileView fileView() const { return FileView(fd_, 0, size_, false); }

  const BufferRef& buffer() const { return buffer_; }

 private:
  size_t size_;
  Buffer buffer_;
  FileDescriptor fd_;
};

class HttpContent::Builder {
 public:
  explicit Builder(size_t bufferSize);

  void write(const BufferRef& chunk);
  void write(FileView&& chunk);
  HttpContent&& commit();

 private:
  void tryDisplaceBufferToFile();

 private:
  size_t bufferSize_;
  size_t size_;
  Buffer buffer_;
  FileDescriptor fd_;
};

} // namespace xzero
} // namespace http
