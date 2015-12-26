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
#include <memory>
#include <deque>

namespace xzero {

class DataChainSink {
 public:
  virtual ~DataChainSink() {}

  virtual size_t transfer(const BufferRef& chunk) = 0;
  virtual size_t transfer(const FileView& chunk) = 0;
};

class DataChain {
 public:
  DataChain();

  void reset();
  void write(const char* cstr);
  void write(const char* buf, size_t n);
  void write(const BufferRef& buf);
  void write(Buffer&& buf);
  void write(FileView&& file);

  /**
   * Transfers as much chained data chunks to @p target as possible.
   *
   * @retval true all data transferred
   * @retval false still data in chain pending
   */
  bool transferTo(DataChainSink* target);

  /**
   * Transfers up to @p n bytes of chained data chunks to @p target.
   *
   * @retval true all @p n bytes of requested data transferred
   * @retval false not all @p n bytes of requested data transferred
   */
  bool transferTo(DataChainSink* target, size_t n);

  bool empty() const noexcept;
  size_t size() const noexcept;

 protected:
  void flushBuffer();

 protected:
  class Chunk;
  class BufferChunk;
  class FileChunk;

  std::deque<std::unique_ptr<Chunk>> chunks_;
  Buffer buffer_;
  size_t size_;
};

// {{{ inlines
inline size_t DataChain::size() const noexcept {
  return size_;
}
// }}}

// {{{ Chunk API
class DataChain::Chunk {
 public:
  virtual ~Chunk() {}

  virtual size_t transferTo(DataChainSink* sink, size_t n) = 0;
  virtual size_t size() const = 0;
};

class DataChain::BufferChunk : public Chunk {
 public:
  explicit BufferChunk(Buffer&& buffer)
      : buffer_(std::forward<Buffer>(buffer)), offset_(0) {}

  size_t transferTo(DataChainSink* sink, size_t n) override;
  size_t size() const override;

 private:
  Buffer buffer_;
  size_t offset_;
};

class DataChain::FileChunk : public Chunk {
 public:
  explicit FileChunk(FileView&& ref)
      : file_(std::forward<FileView>(ref)) {}

  size_t transferTo(DataChainSink* sink, size_t n) override;
  size_t size() const override;

 private:
  FileView file_;
};
// }}}

} // namespace xzero
