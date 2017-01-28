// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/ir/HandlerPass.h>

namespace xzero {
namespace flow {

/**
 * Eliminates empty blocks, that are just jumping to the next block.
 */
class XZERO_FLOW_API EmptyBlockElimination : public HandlerPass {
 public:
  EmptyBlockElimination() : HandlerPass("EmptyBlockElimination") {}

  bool run(IRHandler* handler) override;
};

}  // namespace flow
}  // namespace xzero
