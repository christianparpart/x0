// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/HugeBuffer.h>
#include <xzero/Application.h>
#include <xzero/io/FileUtil.h>

namespace xzero {

HugeBuffer::HugeBuffer(size_t maxBufferSize)
    : maxBufferSize_(maxBufferSize),
      actualSize_(0),
      buffer_(),
      fd_() {
}

HugeBuffer::HugeBuffer(Buffer&& inputBuffer)
    : maxBufferSize_(inputBuffer.size()),
      actualSize_(maxBufferSize_),
      buffer_(std::move(inputBuffer)),
      fd_() {
}

HugeBuffer::HugeBuffer()
    : HugeBuffer(Application::pageSize()) {
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

FileView HugeBuffer::takeFileView() {
  const_cast<HugeBuffer*>(this)->tryDisplaceBufferToFile();

  FileView view = FileView(std::move(fd_), 0, actualSize_);
  actualSize_ = 0;
  return view;
}

const BufferRef& HugeBuffer::getBuffer() const {
  if (buffer_.empty() && fd_.isOpen())
    const_cast<HugeBuffer*>(this)->buffer_ = FileUtil::read(fd_);

  return buffer_;
}

Buffer&& HugeBuffer::takeBuffer() {
  if (buffer_.empty() && fd_.isOpen())
    const_cast<HugeBuffer*>(this)->buffer_ = FileUtil::read(fd_);

  return std::move(buffer_);
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

void HugeBuffer::clear() {
  buffer_.clear();
  actualSize_ = 0;
  fd_.close();
}

} // namespace xzero
