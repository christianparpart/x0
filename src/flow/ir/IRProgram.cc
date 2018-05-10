// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/IRProgram.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/HandlerPass.h>
#include <flow/ir/ConstantArray.h>
#include <assert.h>

namespace xzero::flow {

IRProgram::IRProgram()
    : modules_(),
      trueLiteral_(true, "trueLiteral"),
      falseLiteral_(false, "falseLiteral"),
      numbers_(),
      strings_(),
      ipaddrs_(),
      cidrs_(),
      regexps_(),
      builtinFunctions_(),
      builtinHandlers_(),
      handlers_() {
}

IRProgram::~IRProgram() {
  handlers_.clear();
  constantArrays_.clear();
  numbers_.clear();
  strings_.clear();
  ipaddrs_.clear();
  cidrs_.clear();
  builtinHandlers_.clear();
  builtinFunctions_.clear();
}

void IRProgram::dump() {
  printf("; IRProgram\n");

  for (auto& handler : handlers_)
    handler->dump();
}

IRHandler* IRProgram::createHandler(const std::string& name) {
  handlers_.emplace_back(std::make_unique<IRHandler>(name, this));
  return handlers_.back().get();
}

template <typename T, typename U>
T* IRProgram::get(std::vector<std::unique_ptr<T>>& table, const U& literal) {
  for (size_t i = 0, e = table.size(); i != e; ++i)
    if (table[i]->get() == literal)
      return table[i].get();

  table.emplace_back(std::make_unique<T>(literal));
  return table.back().get();
}

template <typename T, typename U>
T* IRProgram::get(std::vector<T>& table, const U& literal) {
  for (size_t i = 0, e = table.size(); i != e; ++i)
    if (table[i].get() == literal)
      return &table[i];

  table.emplace_back(literal);
  return &table.back();
}

template ConstantInt* IRProgram::get<ConstantInt, int64_t>(
    std::vector<std::unique_ptr<ConstantInt>>&, const int64_t&);

template ConstantArray* IRProgram::get<
    ConstantArray, std::vector<Constant*>>(
        std::vector<ConstantArray>&, const std::vector<Constant*>&);

template ConstantString* IRProgram::get<ConstantString, std::string>(
    std::vector<std::unique_ptr<ConstantString>>&, const std::string&);

template ConstantIP* IRProgram::get<ConstantIP, IPAddress>(
    std::vector<std::unique_ptr<ConstantIP>>&, const IPAddress&);

template ConstantCidr* IRProgram::get<ConstantCidr, Cidr>(
    std::vector<std::unique_ptr<ConstantCidr>>&, const Cidr&);

template ConstantRegExp* IRProgram::get<ConstantRegExp, util::RegExp>(
    std::vector<std::unique_ptr<ConstantRegExp>>&, const util::RegExp&);

}  // namespace xzero::flow
