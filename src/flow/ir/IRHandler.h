// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/ir/Constant.h>
#include <flow/ir/BasicBlock.h>
#include <flow/util/unbox.h>

#include <string>
#include <memory>
#include <list>

namespace xzero::flow {

class BasicBlock;
class IRProgram;
class IRBuilder;

class IRHandler : public Constant {
 public:
  IRHandler(const std::string& name, IRProgram* parent);
  ~IRHandler();

  IRHandler(IRHandler&&) = default;
  IRHandler& operator=(IRHandler&&) = default;

  BasicBlock* createBlock(const std::string& name = "");

  IRProgram* getProgram() const { return program_; }
  void setParent(IRProgram* prog) { program_ = prog; }

  void dump() override;

  auto basicBlocks() { return util::unbox(blocks_); }

  BasicBlock* getEntryBlock() const { return blocks_.front().get(); }
  void setEntryBlock(BasicBlock* bb);

  /**
   * Unlinks and deletes given basic block \p bb from handler.
   *
   * @note \p bb will be a dangling pointer after this call.
   */
  void erase(BasicBlock* bb);

  bool isAfter(const BasicBlock* bb, const BasicBlock* afterThat) const;
  void moveAfter(const BasicBlock* moveable, const BasicBlock* afterThat);
  void moveBefore(const BasicBlock* moveable, const BasicBlock* beforeThat);

  /**
   * Performs given transformation on this handler.
   *
   * @see HandlerPass
   */
  template <typename TheHandlerPass, typename... Args>
  size_t transform(Args&&... args) {
    return TheHandlerPass(std::forward(args)...).run(this);
  }

  /**
   * Performs sanity checks on internal data structures.
   *
   * This call does not return any success or failure as every failure is
   * considered fatal and will cause the program to exit with diagnostics
   * as this is most likely caused by an application programming error.
   *
   * @note Always call this on completely defined handlers and never on
   * half-contructed ones.
   */
  void verify();

 private:
  IRProgram* program_;
  std::list<std::unique_ptr<BasicBlock>> blocks_;

  friend class IRBuilder;
};

}  // namespace xzero::flow
