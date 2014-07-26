// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_io_FileSource_hpp
#define sw_x0_io_FileSource_hpp 1

#include <base/io/Source.h>
#include <base/io/SinkVisitor.h>
#include <string>

namespace base {

//! \addtogroup io
//@{

/** file source.
 */
class BASE_API FileSource : public Source, public SinkVisitor {
 private:
  int handle_;
  off_t offset_;
  size_t count_;
  bool autoClose_;

  ssize_t result_;

 public:
  explicit FileSource(const char* filename);
  FileSource(int fd, off_t offset, std::size_t count, bool autoClose);
  FileSource(FileSource&& other);
  FileSource(const FileSource& other);
  ~FileSource();

  inline int handle() const { return handle_; }
  inline off_t offset() const { return offset_; }
  ssize_t size() const override { return count_; }

  virtual ssize_t sendto(Sink& output);
  virtual const char* className() const;

 protected:
  virtual void visit(BufferSink&);
  virtual void visit(FileSink&);
  virtual void visit(FixedBufferSink&);
  virtual void visit(SocketSink&);
  virtual void visit(PipeSink&);
  virtual void visit(SyslogSink&);
  virtual void visit(LogFile&);
};

//@}

}  // namespace base

#endif
