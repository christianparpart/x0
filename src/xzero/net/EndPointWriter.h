// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Buffer.h>
#include <xzero/io/FileView.h>
#include <xzero/io/DataChain.h>
#include <xzero/io/DataChainListener.h>
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

  DataChain* chain() noexcept { return &chain_; }

 protected:
  size_t transfer(const BufferRef& chunk) override;
  size_t transfer(const FileView& chunk) override;

 private:
  DataChain chain_;
  EndPoint* sink_;
};

} // namespace xzero
