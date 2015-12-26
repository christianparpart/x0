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
#include <xzero/DataChain.h>
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
class XZERO_BASE_API EndPointWriter : public DataChainListener {
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

 protected:
  size_t transfer(const BufferRef& chunk) override;
  size_t transfer(const FileView& chunk) override;

 private:
  DataChain chain_;
  EndPoint* sink_;
};

} // namespace xzero
