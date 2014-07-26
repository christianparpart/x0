// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/Pipe.h>
#include <base/io/PipeSink.h>
#include <base/io/SinkVisitor.h>

namespace base {

PipeSink::PipeSink(Pipe* pipe) : pipe_(pipe) {}

PipeSink::~PipeSink() {}

void PipeSink::accept(SinkVisitor& v) { v.visit(*this); }

ssize_t PipeSink::write(const void* buffer, size_t size) {
  return pipe_->write(buffer, size);
}

}  // namespace base
