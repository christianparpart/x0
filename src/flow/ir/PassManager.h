// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <list>
#include <memory>

namespace xzero::flow {

class HandlerPass;
class IRHandler;
class IRProgram;

class PassManager {
 public:
  PassManager();
  ~PassManager();

  /** registers given pass to the pass manager.
   */
  void registerPass(std::unique_ptr<HandlerPass>&& handlerPass);

  /** runs passes on a complete program.
   */
  void run(IRProgram* program);

  /** runs passes on given handler.
   */
  void run(IRHandler* handler);

 private:
  std::list<std::unique_ptr<HandlerPass>> handlerPasses_;
};

}  // namespace xzero::flow
