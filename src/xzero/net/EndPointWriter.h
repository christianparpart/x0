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
#include <xzero/Buffer.h>
#include <xzero/io/FileView.h>
#include <memory>
#include <deque>

namespace xzero {

class Buffer;
class BufferRef;
class FileView;
class EndPoint;

/**
 * Composable EndPoint Writer API.
 *
 * @todo 2 consecutive buffer writes should merge.
 * @todo consider managing its own BufferPool
 */
class XZERO_BASE_API EndPointWriter {
 public:
  EndPointWriter();
  ~EndPointWriter();

  /**
   * Writes given @p data into the chunk queue.
   */
  void write(const BufferRef& data);

  /**
   * Appends given @p data into the chunk queue.
   */
  void write(Buffer&& data);

  /**
   * Appends given chunk represented by given file descriptor and range.
   *
   * @param file file ref to read from.
   */
  void write(FileView&& file);

  /**
   * Transfers as much data as possible into the given EndPoint @p sink.
   *
   * @retval true all data has been transferred.
   * @retval false data transfer incomplete and data is pending.
   */
  bool flush(EndPoint* sink);

  /** Tests whether there are pending bytes to be flushed.
   *
   * @retval true Yes, we have at least one or more pending bytes to be flushed.
   * @retval false Nothing to be flushed yet.
   */
  bool empty() const;

 private:
  class Chunk;
  class BufferChunk;
  class BufferRefChunk;
  class FileChunk;

  std::deque<std::unique_ptr<Chunk>> chunks_;
};

// {{{ Chunk API
class XZERO_BASE_API EndPointWriter::Chunk {
 public:
  virtual ~Chunk() {}

  virtual bool transferTo(EndPoint* sink) = 0;
  virtual bool empty() const = 0;
};

class XZERO_BASE_API EndPointWriter::BufferChunk : public Chunk {
 public:
  explicit BufferChunk(Buffer&& buffer)
      : data_(std::forward<Buffer>(buffer)), offset_(0) {}

  explicit BufferChunk(const BufferRef& buffer)
      : data_(buffer), offset_(0) {}

  explicit BufferChunk(const Buffer& copy)
      : data_(copy), offset_(0) {}

  bool transferTo(EndPoint* sink) override;
  bool empty() const override;

 private:
  Buffer data_;
  size_t offset_;
};

class XZERO_BASE_API EndPointWriter::BufferRefChunk : public Chunk {
 public:
  explicit BufferRefChunk(const BufferRef& buffer)
      : data_(buffer), offset_(0) {}

  bool transferTo(EndPoint* sink) override;
  bool empty() const override;

 private:
  BufferRef data_;
  size_t offset_;
};

class XZERO_BASE_API EndPointWriter::FileChunk : public Chunk {
 public:
  explicit FileChunk(FileView&& ref)
      : file_(std::forward<FileView>(ref)) {}

  ~FileChunk();

  bool transferTo(EndPoint* sink) override;
  bool empty() const override;

 private:
  FileView file_;
};
// }}}

} // namespace xzero
