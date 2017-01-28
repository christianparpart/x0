// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
#include <vector>
#include <string>
#include <functional>
#include <xzero/reflect/indexsequence.h>

namespace xzero {
namespace reflect {

template <typename ClassType, typename TargetType>
class PropertyReader {
public:
  PropertyReader(TargetType* target);

  template <typename PropertyType>
  void prop(
      PropertyType prop,
      uint32_t id,
      const std::string& prop_name,
      bool optional);

  const ClassType& instance() const;

protected:
  ClassType instance_;
  TargetType* target_;
};

template <typename ClassType, typename TargetType>
class PropertyWriter {
public:
  PropertyWriter(const ClassType& instance, TargetType* target);

  template <typename PropertyType>
  void prop(
      PropertyType prop,
      uint32_t id,
      const std::string& prop_name,
      bool optional);

protected:
  const ClassType& instance_;
  TargetType* target_;
};

template <typename TargetType>
class PropertyProxy {
public:
  PropertyProxy(TargetType* target);

  template <typename PropertyType>
  void prop(
      PropertyType prop,
      uint32_t id,
      const std::string& prop_name,
      bool optional);

protected:
  TargetType* target_;
};

}
}

#include "property_impl.h"
