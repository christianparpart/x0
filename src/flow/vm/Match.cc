// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/vm/Match.h>
#include <flow/vm/Program.h>
#include <flow/vm/Handler.h>
#include <flow/vm/Runner.h>

namespace xzero::flow {

// {{{ Match
Match::Match(const MatchDef& def)
    : def_(def) {
}

Match::~Match() {
}
// }}} Match
// {{{ MatchSame
MatchSame::MatchSame(const MatchDef& def, Program* program)
    : Match(def),
      map_() {
  for (const auto& one : def.cases) {
    map_[program->constants().getString(one.label)] = one.pc;
  }
}

MatchSame::~MatchSame() {
}

uint64_t MatchSame::evaluate(const FlowString* condition, Runner* env) const {
  const auto i = map_.find(*condition);
  if (i != map_.end())
    return i->second;

  return def_.elsePC;  // no match found
}
// }}}
// {{{ MatchHead
MatchHead::MatchHead(const MatchDef& def, Program* program)
    : Match(def),
      map_() {
  for (const auto& one : def.cases) {
    map_.insert(program->constants().getString(one.label), one.pc);
  }
}

MatchHead::~MatchHead() {
}

uint64_t MatchHead::evaluate(const FlowString* condition, Runner* env) const {
  uint64_t result;
  if (map_.lookup(*condition, &result))
    return result;

  return def_.elsePC;  // no match found
}
// }}}
// {{{ MatchTail
MatchTail::MatchTail(const MatchDef& def, Program* program)
    : Match(def),
      map_() {
  for (const auto& one: def.cases) {
    map_.insert(program->constants().getString(one.label), one.pc);
  }
}

MatchTail::~MatchTail() {
}

uint64_t MatchTail::evaluate(const FlowString* condition, Runner* env) const {
  uint64_t result;
  if (map_.lookup(*condition, &result))
    return result;

  return def_.elsePC;  // no match found
}
// }}}
// {{{ MatchRegEx
MatchRegEx::MatchRegEx(const MatchDef& def, Program* program)
    : Match(def), map_() {
  for (const auto& one : def.cases) {
    map_.push_back(
        std::make_pair(program->constants().getRegExp(one.label), one.pc));
  }
}

MatchRegEx::~MatchRegEx() {
}

uint64_t MatchRegEx::evaluate(const FlowString* condition, Runner* env) const {
  util::RegExpContext* cx = (util::RegExpContext*)env->userdata();
  util::RegExp::Result* rs = cx ? cx->regexMatch() : nullptr;
  for (const std::pair<util::RegExp, uint64_t>& one : map_) {
    if (one.first.match(*condition, rs)) {
      return one.second;
    }
  }

  return def_.elsePC;  // no match found
}
// }}}

}  // namespace xzero::flow
