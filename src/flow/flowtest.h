// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/Diagnostics.h>
#include <flow/Params.h>
#include <flow/SourceLocation.h>
#include <flow/vm/Runtime.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <system_error>
#include <vector>

namespace flowtest {

enum class Token {
  Eof,
  Begin,              // "#"
  InitializerMark,    // "----"
  LF,

  TokenError,
  SyntaxError,
  TypeError,
  Warning,
  LinkError,

  Colon,              // ":"
  BrOpen,             // "["
  BrClose,            // "]"
  DotDot,             // ".."
  Number,             // [0-9]+

  MessageText,        // <anything after Location until [^HT] LF>
};

class LexerError : public std::runtime_error {
 public:
  explicit LexerError(const std::string& msg) : std::runtime_error{msg} {}
};

class SyntaxError : public std::runtime_error {
 public:
  explicit SyntaxError(const std::string& msg) : std::runtime_error{msg} {}
};

class Lexer {
 public:
  Lexer();
  Lexer(const std::string& filename, const std::string& contents); 

  const std::string& source() const noexcept { return source_; }
  std::string getPrefixText() const { return source_.substr(0, startOffset_); }

  Token currentToken() const noexcept { return currentToken_; }
  unsigned numberValue() const noexcept { return numberValue_; }
  const std::string& stringValue() const noexcept { return stringValue_; }

  Token nextToken();
  bool consumeIf(Token t);
  void consume(Token t);
  void consumeOneOf(std::initializer_list<Token>&& tokens);
  int consumeNumber();
  std::string consumeText(Token t);

  Lexer& operator++() { nextToken(); }
  bool operator==(const Lexer& other) const noexcept { return currentOffset() == other.currentOffset(); }
  bool operator!=(const Lexer& other) const noexcept { return !(*this == other); }
  bool eof() const noexcept { return currentToken() == Token::Eof; }

 private:
  bool eof_() const noexcept { return currentOffset() == source_.size(); }
  size_t currentOffset() const noexcept { return currentPos_.offset; }
  int currentChar() const { return !eof_() ? source_[currentOffset()] : -1; }

  bool peekSequenceMatch(const std::string& sequence) const;
  int peekChar(off_t i = 1) const {
    return currentOffset() + i < source_.size()
        ? source_[currentOffset() + i]
        : -1;
  }

  int nextChar(off_t i = 1);
  void skipSpace();
  Token parseNumber();
  Token parseIdent();
  Token parseMessageText();

 private:
  std::string filename_;
  std::string source_;
  size_t startOffset_;
  Token currentToken_;
  xzero::flow::FilePos currentPos_;
  unsigned numberValue_;
  std::string stringValue_;
};

using DiagnosticsType = xzero::flow::diagnostics::Type;
using Message = xzero::flow::diagnostics::Message;
using SourceLocation = xzero::flow::SourceLocation;
using FilePos = xzero::flow::FilePos;

/**
 * Parses the input @p contents and splits it into a flow program and a vector
 * of analysis Message.
 */
class Parser {
 public:
  Parser(const std::string& filename, const std::string& contents);

  std::error_code parse(xzero::flow::diagnostics::Report* report);

 private:
  std::string parseUntilInitializer();
  std::string parseLine();
  Message parseMessage();
  DiagnosticsType parseDiagnosticsType();
  SourceLocation tryParseLocation();
  FilePos parseFilePos();

  void reportError(const std::string& msg);

  template<typename... Args>
  void reportError(const std::string& fmt, Args&&... args) {
    reportError(fmt::format(fmt, args...));
  }

 private:
  Lexer lexer_;
};

} // namespace flowtest

namespace fmt {
  template<>
  struct formatter<flowtest::Token> {
    using Token = flowtest::Token;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Token& v, FormatContext &ctx) {
      switch (v) {
        case Token::Eof:
          return format_to(ctx.begin(), "EOF");
        case Token::Begin:
          return format_to(ctx.begin(), "'#'");
        case Token::InitializerMark:
          return format_to(ctx.begin(), "'# ----'");
        case Token::LF:
          return format_to(ctx.begin(), "<LF>");
        case Token::TokenError:
          return format_to(ctx.begin(), "'TokenError'");
        case Token::SyntaxError:
          return format_to(ctx.begin(), "'SyntaxError'");
        case Token::TypeError:
          return format_to(ctx.begin(), "'TypeError'");
        case Token::Warning:
          return format_to(ctx.begin(), "'Warning'");
        case Token::LinkError:
          return format_to(ctx.begin(), "'LinkError'");
        case Token::Colon:
          return format_to(ctx.begin(), "':'");
        case Token::BrOpen:
          return format_to(ctx.begin(), "'['");
        case Token::BrClose:
          return format_to(ctx.begin(), "']'");
        case Token::DotDot:
          return format_to(ctx.begin(), "'..'");
        case Token::Number:
          return format_to(ctx.begin(), "<NUMBER>");
        case Token::MessageText:
          return format_to(ctx.begin(), "<message text>");
        default:
          return format_to(ctx.begin(), "{}", static_cast<unsigned>(v));
      }
    }
  };
}
