// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/PipeSource.h>
#include <base/io/BufferSink.h>
#include <base/io/FileSink.h>
#include <base/io/FixedBufferSink.h>
#include <base/io/SocketSink.h>
#include <base/io/PipeSink.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace base {

PipeSource::PipeSource(Pipe* pipe) : pipe_(pipe) {}

PipeSource::~PipeSource() {}

ssize_t PipeSource::sendto(Sink& output) {
  output.accept(*this);
  return result_;
}

void PipeSource::visit(BufferSink& sink) {
  char buf[8 * 4096];
  result_ = pipe_->read(buf, sizeof(buf));

  if (result_ > 0) {
    sink.write(buf, result_);
  }
}

void PipeSource::visit(FileSink& sink) {
  char buf[8 * 1024];
  result_ = pipe_->read(buf, sizeof(buf));

  if (result_ > 0) {
    sink.write(buf, result_);
  }
}

void PipeSource::visit(FixedBufferSink& v) {
  result_ = pipe_->read(v.buffer().data() + v.buffer().size(),
                        v.buffer().capacity() - v.buffer().size());
}

void PipeSource::visit(SocketSink& sink) {
  result_ = sink.write(pipe_, 8 * 1024);
}

void PipeSource::visit(PipeSink& sink) {
  result_ = sink.write(pipe_, pipe_->size());
}

const char* PipeSource::className() const { return "PipeSource"; }

}  // namespace base
