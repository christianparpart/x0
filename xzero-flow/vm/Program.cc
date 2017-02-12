// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/ConstantPool.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero-flow/vm/Runtime.h>
#include <xzero-flow/vm/Runner.h>
#include <xzero-flow/vm/Match.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <utility>
#include <vector>
#include <memory>
#include <new>

namespace xzero {
namespace flow {
namespace vm {

#define TRACE(msg...) logTrace("flow.vm.Program", msg)

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
  TRACE("Program.ctor");
}

Program::~Program() {
  TRACE("~Program.dtor");
  for (auto m : matches_)
    delete m;
}

std::shared_ptr<Handler> Program::handler(size_t index) const {
  return handlers_[index];
}

void Program::setup() {
  for (const auto& handler : cp_.getHandlers())
    createHandler(handler.first, handler.second);

  const std::vector<MatchDef>& matches = cp_.getMatchDefs();
  for (size_t i = 0, e = matches.size(); i != e; ++i) {
    const MatchDef& def = matches[i];
    switch (def.op) {
      case MatchClass::Same:
        matches_.emplace_back(new MatchSame(def, shared_from_this()));
        break;
      case MatchClass::Head:
        matches_.emplace_back(new MatchHead(def, shared_from_this()));
        break;
      case MatchClass::Tail:
        matches_.emplace_back(new MatchTail(def, shared_from_this()));
        break;
      case MatchClass::RegExp:
        matches_.emplace_back(new MatchRegEx(def, shared_from_this()));
        break;
    }
  }
}

std::shared_ptr<Handler> Program::createHandler(const std::string& name) {
  return createHandler(name, {});
}

std::shared_ptr<Handler> Program::createHandler(
    const std::string& name,
    const std::vector<Instruction>& instructions) {
  auto handler = std::make_shared<Handler>(shared_from_this(),
                                           name,
                                           instructions);
  handlers_.emplace_back(handler);
  return handler;
}

std::shared_ptr<Handler> Program::findHandler(const std::string& name) const {
  for (auto& handler: handlers_) {
    if (handler->name() == name)
      return handler;
  }

  return nullptr;
}

bool Program::run(const std::string& handlerName, void* u1, void* u2) {
  if (std::shared_ptr<Handler> handler = findHandler(handlerName))
    return handler->run(u1, u2);

  RAISE(RuntimeError, "No handler with name '%s' found.", handlerName.c_str());
}

std::vector<std::string> Program::handlerNames() const {
  std::vector<std::string> result;
  result.reserve(handlers_.size());

  for (auto& handler: handlers_)
    result.emplace_back(handler->name());

  return result;
}

int Program::indexOf(const std::shared_ptr<Handler>& that) const {
  for (int i = 0, e = handlers_.size(); i != e; ++i)
    if (handler(i).get() == that.get())
      return i;

  return -1;
}

int Program::indexOf(const Handler* that) const {
  for (int i = 0, e = handlers_.size(); i != e; ++i)
    if (handler(i).get() == that)
      return i;

  return -1;
}

void Program::dump() {
  printf("; Program\n");

  cp_.dump();

  for (size_t i = 0, e = handlers_.size(); i != e; ++i) {
    std::shared_ptr<Handler> handler = this->handler(i);
    printf("\n.handler %-20s ; #%zu (%zu registers, %zu instructions)\n",
           handler->name().c_str(), i,
           handler->registerCount() ? handler->registerCount() - 1
                                    : 0,  // r0 is never used
           handler->code().size());
    handler->disassemble();
  }

  printf("\n\n");
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
bool Program::link(Runtime* runtime) {
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
      logError("flow.vm.Program",
               "Unresolved native handler signature: $0",
               signature);
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
      logError("flow.vm.Program",
               "Unresolved native function signature: %s\n",
               signature);
      // TODO unresolvedSymbols_.push_back(signature);
      errors++;
    }
    ++i;
  }

  return errors == 0;
}

}  // namespace vm
}  // namespace flow
}  // namespace xzero
