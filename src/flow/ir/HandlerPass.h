// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero::flow {

class IRHandler;

class HandlerPass {
 public:
  explicit HandlerPass(const char* name) : name_(name) {}

  virtual ~HandlerPass() {}

  /**
   * Runs this pass on given \p handler.
   *
   * @retval true The handler was modified.
   * @retval false The handler was not modified.
   */
  virtual bool run(IRHandler* handler) = 0;

  const char* name() const { return name_; }

 private:
  const char* name_;
};

}  // namespace xzero::flow
