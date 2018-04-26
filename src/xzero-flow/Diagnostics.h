// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/SourceLocation.h>

#include <fmt/format.h>
#include <string>
#include <system_error>
#include <vector>

namespace xzero::flow::diagnostics {

enum class Type {
  TokenError,
  SyntaxError,
  TypeError,
  Warning,
  LinkError
};

struct Message {
  Type type;
  SourceLocation sourceLocation;
  std::vector<std::string> texts;

  bool operator==(const Message& other) const noexcept;
  bool operator!=(const Message& other) const noexcept { return !(*this == other); }
};

using MessageList = std::vector<Message>;

class DiagnosticsError : public std:runtime_error {
 public:
  explicit DiagnosticsError(SourceLocation sloc, const std::string& msg) : std::runtime_error{msg} {}

  const SourceLocation& sourceLocation() const noexcept { return sloc_; }

 private:
  SourceLocation sloc_;
}

class LexerError : public DiagnosticsError {
 public:
  LexerError(SourceLocation sloc, const std::string& msg) : DiagnosticsError{sloc, msg} {}
};

class SyntaxError : public DiagnosticsError {
 public:
  SyntaxError(SourceLocation sloc, const std::string& msg) : DiagnosticsError{sloc, msg} {}
};

class TypeError : public DiagnosticsError {
 public:
  TypeError(SourceLocation sloc, const std::string& msg) : DiagnosticsError{sloc, msg} {}
};

} // namespace xzero::flow

namespace fmt {
  template<>
  struct formatter<xzero::flow::diagnostics::Type> {
    using Type = xzero::flow::diagnostics::Type;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Type& v, FormatContext &ctx) {
      switch (v) {
        case Type::TokenError:
          return format_to(ctx.begin(), "TokenError");
        case Type::SyntaxError:
          return format_to(ctx.begin(), "SyntaxError");
        case Type::TypeError:
          return format_to(ctx.begin(), "TypeError");
        case Type::Warning:
          return format_to(ctx.begin(), "Warning");
        case Type::LinkError:
          return format_to(ctx.begin(), "LinkError");
        default:
          return format_to(ctx.begin(), "{}", static_cast<unsigned>(v));
      }
    }
  };
}
