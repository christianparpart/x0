// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <utility>
#include <memory>
#include <iosfwd>
#include <cstdint>
#include <fmt/format.h>

namespace xzero::flow {

//! \addtogroup Flow
//@{

enum class FlowToken {
  Unknown,

  // literals
  Boolean,
  Number,
  String,
  RawString,
  RegExp,
  IP,
  Cidr,
  NamedParam,
  InterpolatedStringFragment,  // "hello #{" or "} world #{"
  InterpolatedStringEnd,       // "} end"

  // symbols
  Assign,
  OrAssign,
  AndAssign,
  PlusAssign,
  MinusAssign,
  MulAssign,
  DivAssign,
  Semicolon,
  Question,
  Colon,
  And,
  Or,
  Xor,
  Equal,
  UnEqual,
  Less,
  Greater,
  LessOrEqual,
  GreaterOrEqual,
  PrefixMatch,
  SuffixMatch,
  RegexMatch,
  In,
  HashRocket,
  Plus,
  Minus,
  Mul,
  Div,
  Mod,
  Shl,
  Shr,
  Comma,
  Pow,
  Not,
  BitNot,
  BitOr,
  BitAnd,
  BitXor,
  BrOpen,
  BrClose,
  RndOpen,
  RndClose,
  Begin,
  End,

  // keywords
  Var,
  Do,
  Handler,
  If,
  Then,
  Else,
  Unless,
  Match,
  On,
  For,
  Import,
  From,

  // data types
  VoidType,
  BoolType,
  NumberType,
  StringType,

  // misc
  Ident,
  Period,
  DblPeriod,
  Ellipsis,
  Comment,
  Eof,
  COUNT
};

class FlowTokenTraits {
 public:
  static bool isKeyword(FlowToken t);
  static bool isReserved(FlowToken t);
  static bool isLiteral(FlowToken t);
  static bool isType(FlowToken t);

  static bool isOperator(FlowToken t);
  static bool isUnaryOp(FlowToken t);
  static bool isPrimaryOp(FlowToken t);
  static bool isRelOp(FlowToken t);
};

std::string to_string(FlowToken t);

//!@}

}  // namespace xzero::flow

namespace std {
  template <>
  struct hash<xzero::flow::FlowToken> {
    uint32_t operator()(xzero::flow::FlowToken v) const {
      return static_cast<uint32_t>(v);
    }
  };
}

namespace fmt {
  template<>
  struct formatter<xzero::flow::FlowToken> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::flow::FlowToken& v, FormatContext &ctx) {
      return format_to(ctx.begin(), to_string(v));
    }
  };
}
