// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <vector>
#include <string>

namespace xzero {

class XZERO_BASE_API StackTrace {
 public:
  StackTrace();
  StackTrace(StackTrace&&);
  StackTrace& operator=(StackTrace&&);
  StackTrace(const StackTrace&);
  StackTrace& operator=(const StackTrace&);
  ~StackTrace();

  std::vector<std::string> symbols() const;

  static std::string demangleSymbol(const char* symbol);

 private:
  void** frames_;
  int frameCount_;
};

} // namespace xzero
