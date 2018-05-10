// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/LiteralType.h>
#include <flow/MatchClass.h>
#include <flow/util/PrefixTree.h>
#include <flow/util/RegExp.h>
#include <flow/util/SuffixTree.h>
#include <flow/vm/Instruction.h>

#include <cstdint>
#include <memory>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

namespace xzero::flow {

struct MatchCaseDef {
  //!< offset into the string pool (or regexp pool) of the associated program.
  uint64_t label;
  //!< program offset into the associated handler
  uint64_t pc;

  MatchCaseDef() = default;
  explicit MatchCaseDef(uint64_t l) : label(l), pc(0) {}
  MatchCaseDef(uint64_t l, uint64_t p) : label(l), pc(p) {}
};

class MatchDef {
 public:
  size_t handlerId;
  MatchClass op;  // == =^ =$ =~
  uint64_t elsePC;
  std::vector<MatchCaseDef> cases;
};

class Program;
class Handler;
class Runner;

class Match {
 public:
  explicit Match(const MatchDef& def);
  virtual ~Match();

  const MatchDef& def() const { return def_; }

  /**
   * Matches input condition.
   * \return a code pointer to continue processing
   */
  virtual uint64_t evaluate(const FlowString* condition, Runner* env) const = 0;

 protected:
  MatchDef def_;
};

/** Implements SMATCHEQ instruction. */
class MatchSame : public Match {
 public:
  MatchSame(const MatchDef& def, Program* program);
  ~MatchSame();

  uint64_t evaluate(const FlowString* condition, Runner* env) const override;

 private:
  std::unordered_map<FlowString, uint64_t> map_;
};

/** Implements SMATCHBEG instruction. */
class MatchHead : public Match {
 public:
  MatchHead(const MatchDef& def, Program* program);
  ~MatchHead();

  uint64_t evaluate(const FlowString* condition, Runner* env) const override;

 private:
  util::PrefixTree<FlowString, uint64_t> map_;
};

/** Implements SMATCHBEG instruction. */
class MatchTail : public Match {
 public:
  MatchTail(const MatchDef& def, Program* program);
  ~MatchTail();

  uint64_t evaluate(const FlowString* condition, Runner* env) const override;

 private:
  util::SuffixTree<FlowString, uint64_t> map_;
};

/** Implements SMATCHR instruction. */
class MatchRegEx : public Match {
 public:
  MatchRegEx(const MatchDef& def, Program* program);
  ~MatchRegEx();

  uint64_t evaluate(const FlowString* condition, Runner* env) const override;

 private:
  std::vector<std::pair<util::RegExp, uint64_t>> map_;
};

}  // namespace xzero::flow
