// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_StackTrace_h
#define sw_x0_StackTrace_h 1

#include <base/Api.h>
#include <base/Buffer.h>
#include <vector>
#include <string>

namespace base {

class BASE_API StackTrace {
 private:
  Buffer buffer_;
  void **addresses_;
  std::vector<BufferRef> symbols_;
  int skip_;
  int count_;

 public:
  explicit StackTrace(int numSkipFrames = 0, int numMaxFrames = 64);
  ~StackTrace();

  void generate(bool verbose);

  int length() const;
  const BufferRef &at(int index) const;

  const char *c_str() const;
};

}  // namespace base

#endif
