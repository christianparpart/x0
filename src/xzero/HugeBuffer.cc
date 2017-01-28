// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/HugeBuffer.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/FileInputStream.h>
#include <xzero/io/BufferInputStream.h>

namespace xzero {

HugeBuffer::HugeBuffer(size_t maxBufferSize)
    : maxBufferSize_(maxBufferSize),
      actualSize_(0),
      buffer_(),
      fd_() {
}

void HugeBuffer::write(const BufferRef& chunk) {
  if (buffer_.size() + chunk.size() > maxBufferSize_)
    tryDisplaceBufferToFile();

  if (fd_.isOpen())
    FileUtil::write(fd_, chunk);
  else
    buffer_.push_back(chunk);

  actualSize_ += chunk.size();
}

void HugeBuffer::write(const FileView& chunk) {
  if (buffer_.size() + chunk.size() > maxBufferSize_)
    tryDisplaceBufferToFile();

  if (fd_.isOpen())
    FileUtil::write(fd_, chunk);
  else
    FileUtil::read(chunk, &buffer_);

  actualSize_ += chunk.size();
}

void HugeBuffer::write(FileView&& chunk) {
  if (actualSize_ == 0 && chunk.offset() == 0) {
    actualSize_ = chunk.size();
    fd_ = chunk.release();
  } else {
    if (buffer_.size() + chunk.size() > maxBufferSize_)
      tryDisplaceBufferToFile();

    if (fd_.isOpen())
      FileUtil::write(fd_, chunk);
    else
      FileUtil::read(chunk, &buffer_);

    actualSize_ += chunk.size();
  }
}

void HugeBuffer::write(Buffer&& chunk) {
  // XXX we shamelessly ignore maxBufferSize here
  if (actualSize_ == 0) {
    buffer_ = std::move(chunk);
    actualSize_ = buffer_.size();
  } else {
    buffer_.push_back(chunk);
    actualSize_ += chunk.size();
  }
}

FileView HugeBuffer::getFileView() const {
  const_cast<HugeBuffer*>(this)->tryDisplaceBufferToFile();

  return FileView(fd_, 0, actualSize_, false);
}

const BufferRef& HugeBuffer::getBuffer() const {
  if (buffer_.empty() && fd_.isOpen())
    const_cast<HugeBuffer*>(this)->buffer_ = FileUtil::read(fd_);

  return buffer_;
}

std::unique_ptr<InputStream> HugeBuffer::getInputStream() {
  if (fd_.isOpen()) {
    // TODO: provide a PositionalFileInputStream (pread's) to get rid of this side-effect
    FileUtil::seek(fd_, 0);
  }

  return std::unique_ptr<InputStream>(
      fd_.isClosed()
          ? static_cast<InputStream*>(new BufferInputStream(&buffer_))
          : static_cast<InputStream*>(new FileInputStream(fd_, false)));
}

void HugeBuffer::tryDisplaceBufferToFile() {
  if (fd_.isClosed()) {
    fd_ = FileUtil::createTempFile();

    if (fd_.isOpen() && !buffer_.empty()) {
      FileUtil::write(fd_, buffer_);
      buffer_.clear();
    }
  }
}

void HugeBuffer::reset() {
  buffer_.clear();
  actualSize_ = 0;
  fd_.close();
}


} // namespace xzero
