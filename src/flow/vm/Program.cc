// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/Diagnostics.h>
#include <flow/vm/ConstantPool.h>
#include <flow/vm/Handler.h>
#include <flow/vm/Instruction.h>
#include <flow/vm/Match.h>
#include <flow/vm/Program.h>
#include <flow/vm/Runner.h>
#include <flow/vm/Runtime.h>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace xzero::flow {

/* {{{ possible binary file format
 * ----------------------------------------------
 * u32                  magic number (0xbeafbabe)
 * u32                  version
 * u64                  flags
 * u64                  register count
 * u64                  code start
 * u64                  code size
 * u64                  integer const-table start
 * u64                  integer const-table element count
 * u64                  string const-table start
 * u64                  string const-table element count
 * u64                  regex const-table start (stored as string)
 * u64                  regex const-table element count
 * u64                  debug source-lines-table start
 * u64                  debug source-lines-table element count
 *
 * u32[]                code segment
 * u64[]                integer const-table segment
 * u64[]                string const-table segment
 * {u32, u8[]}[]        strings
 * {u32, u32, u32}[]    debug source lines segment
 */  // }}}

Program::Program(ConstantPool&& cp)
    : cp_(std::move(cp)),
      runtime_(nullptr),
      handlers_(),
      matches_(),
      nativeHandlers_(),
      nativeFunctions_() {
  setup();
}

Program::~Program() {
}

Handler* Program::handler(size_t index) const {
  return handlers_[index].get();
}

void Program::setup() {
  for (const auto& handler : cp_.getHandlers())
    createHandler(handler.first, handler.second);

  const std::vector<MatchDef>& matches = cp_.getMatchDefs();
  for (size_t i = 0, e = matches.size(); i != e; ++i) {
    const MatchDef& def = matches[i];
    switch (def.op) {
      case MatchClass::Same:
        matches_.emplace_back(std::make_unique<MatchSame>(def, this));
        break;
      case MatchClass::Head:
        matches_.emplace_back(std::make_unique<MatchHead>(def, this));
        break;
      case MatchClass::Tail:
        matches_.emplace_back(std::make_unique<MatchTail>(def, this));
        break;
      case MatchClass::RegExp:
        matches_.emplace_back(std::make_unique<MatchRegEx>(def, this));
        break;
    }
  }
}

Handler* Program::createHandler(const std::string& name) {
  return createHandler(name, {});
}

Handler* Program::createHandler(const std::string& name, const Code& code) {
  handlers_.emplace_back(std::make_unique<Handler>(this,
                                                   name,
                                                   code));
  return handlers_.back().get();
}

Handler* Program::findHandler(const std::string& name) const noexcept {
  for (auto& handler: handlers_)
    if (handler->name() == name)
      return handler.get();

  return nullptr;
}

std::vector<std::string> Program::handlerNames() const {
  std::vector<std::string> result;
  result.reserve(handlers_.size());

  for (auto& handler: handlers_)
    result.emplace_back(handler->name());

  return result;
}

int Program::indexOf(const Handler* that) const noexcept {
  for (int i = 0, e = handlers_.size(); i != e; ++i)
    if (handlers_[i].get() == that)
      return i;

  return -1;
}

void Program::dump() {
  cp_.dump();
}

/**
 * Maps all native functions/handlers to their implementations (report
 *unresolved symbols)
 *
 * \param runtime the runtime to link this program against, resolving any
 *external native symbols.
 * \retval true Linking succeed.
 * \retval false Linking failed due to unresolved native signatures not found in
 *the runtime.
 */
bool Program::link(Runtime* runtime, diagnostics::Report* report) {
  runtime_ = runtime;
  int errors = 0;

  // load runtime modules
  for (const auto& module : cp_.getModules()) {
    if (!runtime->import(module.first, module.second, nullptr)) {
      errors++;
    }
  }

  // link nattive handlers
  nativeHandlers_.resize(cp_.getNativeHandlerSignatures().size());
  size_t i = 0;
  for (const auto& signature : cp_.getNativeHandlerSignatures()) {
    // map to nativeHandlers_[i]
    if (NativeCallback* cb = runtime->find(signature)) {
      nativeHandlers_[i] = cb;
    } else {
      nativeHandlers_[i] = nullptr;
      report->linkError("Unresolved symbol to native handler signature: {}", signature);
      // TODO unresolvedSymbols_.push_back(signature);
      errors++;
    }
    ++i;
  }

  // link nattive functions
  nativeFunctions_.resize(cp_.getNativeFunctionSignatures().size());
  i = 0;
  for (const auto& signature : cp_.getNativeFunctionSignatures()) {
    if (NativeCallback* cb = runtime->find(signature)) {
      nativeFunctions_[i] = cb;
    } else {
      nativeFunctions_[i] = nullptr;
      report->linkError("Unresolved native function signature: {}", signature);
      errors++;
    }
    ++i;
  }

  return errors == 0;
}

}  // namespace xzero::flow
