// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/io/FileView.h>
#include <xzero/io/FileDescriptor.h>
#include <memory>

namespace xzero {

class InputStream;

/**
 * A huge buffer API that can contain more data than your RAM.
 *
 * It first attempts to store the data in memory, with
 * up to @c n bytes, as specified in the constructor.
 *
 * Once this threshold is being exceeded, the buffer is
 * swapped onto a temporary disk location and further bytes
 * will be written to disk, too.
 */
class HugeBuffer {
 public:
  /**
   * Initializes an empty HugeBuffer instance with the in-memory capacity
   * set to @p maxBufferSize.
   *
   * @param maxBufferSize Maximum capacity for the in-memory buffer.
   */
  explicit HugeBuffer(size_t maxBufferSize);

  /**
   * Initializes this instance with the given @p inputBuffer.
   *
   * The instance's maxBufferSize will be set to the @p inputBuffer's size.
   *
   * @param inputBuffer The buffer that is to be moved into this HugeBuffer.
   */
  HugeBuffer(Buffer&& inputBuffer);

  /**
   * Initializes this instance with the system default page-size as
   * maxBufferSize.
   */
  HugeBuffer();

  /** Retrieves the maximum size that may be kept in memory. */
  size_t maxBufferSize() const noexcept { return maxBufferSize_; }

  /**
   * Tests whether this HugeBuffer is empty or not.
   */
  bool empty() const noexcept { return actualSize_ == 0; }

  /**
   * Retrieves the actual number of bytes within this HugeBuffer.
   */
  size_t size() const noexcept { return actualSize_; }

  /**
   * Tests whether this HugeBuffer is layed off into a temporary file.
   */
  bool isFile() const noexcept { return fd_.isOpen(); }

  /**
   * Tests whether this HugeBuffer is buffered in-memory.
   */
  bool isBuffered() const noexcept { return !buffer_.empty(); }

  /**
   * Retrieves a caller-owned InputStream to read out this HugeBuffer.
   */
  std::unique_ptr<InputStream> getInputStream();

  /**
   * Retrieves a FileView representation to this HugeBuffer.
   *
   * This persists the buffer into a temporary file if currently only in-memory,
   * so you can access this HugeBuffer via a FileView.
   */
  FileView getFileView() const;

  /**
   * Retrieves the FileView representation to this HugeBuffer, moving ownership
   * to caller.
   *
   * This persists the buffer into a temporary file if currently only in-memory,
   * so you can access this HugeBuffer via a FileView.
   */
  FileView&& getFileView();

  /**
   * Retrieves a reference to the internal buffer of HugeBuffer.
   *
   * If the data is currently backed by a temporary file, it will be
   * loaded into memory, so you can access it via BufferRef.
   */
  const BufferRef& getBuffer() const;
  Buffer&& getBuffer();

  void write(const BufferRef& chunk);
  void write(const FileView& chunk);
  void write(FileView&& chunk);
  void write(Buffer&& chunk);

  void reset();

 private:
  void tryDisplaceBufferToFile();

 private:
  size_t maxBufferSize_;
  size_t actualSize_;
  Buffer buffer_;
  FileDescriptor fd_;
};

} // namespace xzero
