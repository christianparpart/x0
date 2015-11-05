// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/http/HttpInput.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {
namespace mock {

class Input : public HttpInput {
 public:
  Input();
  int read(Buffer* result) override;
  size_t readLine(Buffer* result) override;
  bool empty() const noexcept override;
  void onContent(const BufferRef& chunk) override;
  void recycle() override;

 private:
  Buffer buffer_;
};

} // namespace mock
} // namespace http
} // namespace xzero
