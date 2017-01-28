// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/ir/Constant.h>

#include <string>
#include <vector>

namespace xzero {
namespace flow {

class XZERO_FLOW_API ConstantArray : public Constant {
 public:
  ConstantArray(const std::vector<Constant*>& elements,
                const std::string& name = "")
      : Constant(makeArrayType(elements.front()->type()), name),
        elements_(elements) {}

  const std::vector<Constant*>& get() const { return elements_; }

  FlowType elementType() const { return elements_[0]->type(); }

 private:
  std::vector<Constant*> elements_;

  FlowType makeArrayType(FlowType elementType);
};

}  // namespace flow
}  // namespace xzero
