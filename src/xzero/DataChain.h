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
 protected:
  class Chunk;
  class BufferChunk;
  class FileChunk;

 public:
  DataChain();

  void reset();

  void write(const char* cstr);
  void write(const char* buf, size_t n);
  void write(const BufferRef& buf);
  void write(Buffer&& buf);
  void write(FileView&& file);
  void write(std::unique_ptr<Chunk>&& chunk);

  void write8(uint8_t bin);
  void write16(uint16_t bin);
  void write24(uint32_t bin);
  void write32(uint32_t bin);

  /**
   * Splits up to @p n bytes data from the front chunk of the data chain.
   *
   * The chunk is potentially cut to meet the byte requirements.
   *
   * @note This method only operates on the front chunk, never on many.
   *
   * @return the given chunk or @c nullptr if none available.
   */
  std::unique_ptr<Chunk> get(size_t n);

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
  std::deque<std::unique_ptr<Chunk>> chunks_;
  Buffer buffer_;
  size_t size_;
};

class DataChain::Chunk {
 public:
  virtual ~Chunk() {}

  virtual std::unique_ptr<Chunk> get(size_t n) = 0;
  virtual size_t transferTo(DataChainSink* sink, size_t n) = 0;
  virtual size_t size() const = 0;
};

// {{{ inlines
inline size_t DataChain::size() const noexcept {
  return size_;
}
// }}}

} // namespace xzero
