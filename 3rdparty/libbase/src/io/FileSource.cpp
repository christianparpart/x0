// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/FileSource.h>
#include <base/io/BufferSink.h>
#include <base/io/FixedBufferSink.h>
#include <base/io/FileSink.h>
#include <base/io/SocketSink.h>
#include <base/io/PipeSink.h>
#include <base/io/SyslogSink.h>
#include <base/LogFile.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace base {

FileSource::FileSource(const char* filename)
    : handle_(::open(filename, O_RDONLY)),
      offset_(0),
      count_(0),
      autoClose_(true) {
  if (handle() > 0) {
    struct stat st;
    if (stat(filename, &st) == 0) count_ = st.st_size;
  }
}

/** initializes a file source.
 *
 * \param f the file to stream
 */
FileSource::FileSource(int fd, off_t offset, std::size_t count, bool autoClose)
    : handle_(fd), offset_(offset), count_(count), autoClose_(autoClose) {}

FileSource::FileSource(const FileSource& other)
    : handle_(other.handle_ >= 0 ? dup(other.handle_) : -1),
      offset_(other.offset_),
      count_(other.count_),
      autoClose_(handle_ >= 0) {}

FileSource::FileSource(FileSource&& other)
    : handle_(other.handle_),
      offset_(other.offset_),
      count_(other.count_),
      autoClose_(other.autoClose_) {
  other.handle_ = -1;
  other.offset_ = 0;
  other.count_ = 0;
  other.autoClose_ = false;
}

FileSource::~FileSource() {
  if (autoClose_) ::close(handle_);
}

ssize_t FileSource::sendto(Sink& output) {
  output.accept(*this);
  return result_;
}

void FileSource::visit(BufferSink& v) {
  char buf[8 * 4096];
  result_ = pread(handle(), buf, sizeof(buf), offset_);

  if (result_ > 0) {
    v.write(buf, result_);
    offset_ += result_;
    count_ -= result_;
  }
}

void FileSource::visit(FileSink& v) {
#if 0
    // only works if target is a pipe.
    loff_t offset = offset_;
    result_ = ::splice(handle(), &offset, v.handle(), NULL, count_,
        SPLICE_F_MOVE | SPLICE_F_NONBLOCK | SPLICE_F_MORE);

    if (result_ > 0) {
        offset_ = offset;
        count_ -= result_;
    }
#endif
  char buf[8 * 1024];
  result_ = pread(handle(), buf, sizeof(buf), offset_);

  if (result_ <= 0) return;

  result_ = v.write(buf, result_);

  if (result_ <= 0) return;

  offset_ += result_;
  count_ -= result_;
}

void FileSource::visit(FixedBufferSink& v) {
  result_ = pread(handle(), v.buffer().data() + v.buffer().size(),
                  v.buffer().capacity() - v.buffer().size(), offset_);

  if (result_ > 0) {
    v.buffer().resize(v.buffer().size() + result_);
    offset_ += result_;
    count_ -= result_;
  }
}

void FileSource::visit(SocketSink& v) {
  result_ = v.write(handle(), &offset_, count_);

  if (result_ > 0) {
    count_ -= result_;
  }
}

void FileSource::visit(PipeSink& sink) {
  result_ = sink.pipe()->write(handle(), &offset_, count_);

  if (result_ > 0) {
    count_ -= result_;
  }
}

void FileSource::visit(SyslogSink& sink) {
  char buf[8 * 1024];
  result_ = pread(handle(), buf, sizeof(buf) - 1, offset_);

  if (result_ <= 0) return;

  result_ = sink.write(buf, result_);

  if (result_ <= 0) return;

  offset_ += result_;
  count_ -= result_;
}

void FileSource::visit(LogFile& sink) {
  char buf[8 * 1024];
  result_ = pread(handle(), buf, sizeof(buf) - 1, offset_);

  if (result_ <= 0) return;

  result_ = sink.write(buf, result_);

  if (result_ <= 0) return;

  offset_ += result_;
  count_ -= result_;
}

const char* FileSource::className() const { return "FileSource"; }

}  // namespace base
