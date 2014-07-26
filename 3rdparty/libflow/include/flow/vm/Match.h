// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/Api.h>
#include <flow/vm/Instruction.h>
#include <flow/vm/MatchClass.h>
#include <flow/FlowType.h>
#include <base/PrefixTree.h>
#include <base/SuffixTree.h>
#include <base/RegExp.h>
#include <sys/types.h>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace flow {
namespace vm {

using namespace base;

struct FLOW_API MatchCaseDef {
  //!< offset into the string pool (or regexp pool) of the associated program.
  uint64_t label;
  //!< program offset into the associated handler
  uint64_t pc;

  MatchCaseDef() = default;
  MatchCaseDef(uint64_t l, uint64_t p) : label(l), pc(p) {}
};

struct FLOW_API MatchDef {
  size_t handlerId;
  MatchClass op;  // == =^ =$ =~
  uint64_t elsePC;
  std::vector<MatchCaseDef> cases;
};

class Program;
class Handler;
class Runner;

class FLOW_API Match {
 public:
  Match(const MatchDef& def, Program* program);
  virtual ~Match();

  const MatchDef& def() const { return def_; }

  /**
   * Matches input condition.
   * \return a code pointer to continue processing
   */
  virtual uint64_t evaluate(const FlowString* condition, Runner* env) const = 0;

 protected:
  MatchDef def_;
  Program* program_;
  Handler* handler_;
  uint64_t elsePC_;
};

/** Implements SMATCHEQ instruction. */
class FLOW_API MatchSame : public Match {
 public:
  MatchSame(const MatchDef& def, Program* program);
  ~MatchSame();

  virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

 private:
  std::unordered_map<FlowString, uint64_t> map_;
};

/** Implements SMATCHBEG instruction. */
class FLOW_API MatchHead : public Match {
 public:
  MatchHead(const MatchDef& def, Program* program);
  ~MatchHead();

  virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

 private:
  PrefixTree<FlowString, uint64_t> map_;
};

/** Implements SMATCHBEG instruction. */
class FLOW_API MatchTail : public Match {
 public:
  MatchTail(const MatchDef& def, Program* program);
  ~MatchTail();

  virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

 private:
  SuffixTree<FlowString, uint64_t> map_;
};

/** Implements SMATCHR instruction. */
class FLOW_API MatchRegEx : public Match {
 public:
  MatchRegEx(const MatchDef& def, Program* program);
  ~MatchRegEx();

  virtual uint64_t evaluate(const FlowString* condition, Runner* env) const;

 private:
  std::vector<std::pair<const RegExp*, uint64_t>> map_;
};

}  // namespace vm
}  // namespace flow
