// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/StackTrace.h>
#include <base/NativeSymbol.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <strings.h>

namespace base {

StackTrace::StackTrace(int numSkipFrames, int numMaxFrames) {
  skip_ = 1 + numSkipFrames;
  // buffer_.resize(numSkipFrames + numMaxFrames);
  addresses_ = new void *[skip_ + numMaxFrames];
  count_ = backtrace(addresses_, skip_ + numMaxFrames);
}

StackTrace::~StackTrace() { delete[] addresses_; }

void StackTrace::generate(bool verbose) {
  if (!symbols_.empty()) return;

  for (int i = skip_; i < count_; ++i) {
    void *address = addresses_[i];

    buffer_.push_back('[');
    buffer_.push_back(i - skip_);
    buffer_.push_back("] ");

    std::size_t begin = buffer_.size();
    buffer_.reserve(buffer_.size() + 512);

    buffer_ << NativeSymbol(address /*, verbose*/);

    std::size_t count = buffer_.size() - begin;
    symbols_.push_back(buffer_.ref(begin, count));

    buffer_.push_back("\n");
  }
}

/** retrieves the number of frames captured. */
int StackTrace::length() const { return count_; }

/** retrieves the frame information of a frame at given index. */
const BufferRef &StackTrace::at(int index) const { return symbols_[index]; }

/** retrieves full stack trace information (of all captured frames). */
const char *StackTrace::c_str() const {
  const_cast<StackTrace *>(this)->generate(true);

  return const_cast<StackTrace *>(this)->buffer_.c_str();
}

}  // namespace base
