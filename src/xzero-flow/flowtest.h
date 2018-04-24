// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Params.h>
#include <xzero-flow/SourceLocation.h>
#include <xzero-flow/vm/Runtime.h>
#include <xzero/Result.h>

#include <cstdint>
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

enum class AnalysisType {
  TokenError,
  SyntaxError,
  TypeError,
  Warning,
  LinkError
};

struct Message {
  AnalysisType type;
  xzero::flow::SourceLocation sourceLocation;
  std::vector<std::string> texts;
};

using MessageList = std::vector<Message>;

struct ParseResult {
  std::string program;
  MessageList messages;
};

struct LexerError {
};

class Lexer {
 public:
  Lexer(const std::string& filename, const std::string& contents); 

  bool eof() const noexcept { return currentOffset() == source_.size(); }
  size_t currentOffset() const noexcept { return currentPos_.offset; }
  int currentChar() const { return !eof() ? source_[currentOffset()] : -1; }

  int peekChar(off_t i = 1) const {
    return currentOffset() + i < source_.size()
        ? source_[currentOffset() + i]
        : -1;
  }

  int nextChar(off_t i = 1);
  Token currentToken() const noexcept { return currentToken_; }
  Token nextToken();
  void skipSpace();
  Token parseNumber();
  Token parseIdent();

 private:
  std::string filename_;
  std::string source_;
  Token currentToken_;
  xzero::flow::FilePos currentPos_;
};

/**
 * Parses the input @p contents and splits it into a flow program and a vector
 * of analysis Message.
 */
class Parser {
 public:
  Parser(const std::string& filename, const std::string& contents);

  Result<ParseResult> parse();

 private:
  // lexer
  bool eof() const noexcept { return currentOffset() == source_.size(); }
  size_t currentOffset() const noexcept { return currentPos_.offset; }
  int currentChar() const { return !eof() ? source_[currentOffset()] : -1; }
  int peekChar(off_t i = 1) const {
    return currentOffset() + i < source_.size()
        ? source_[currentOffset() + i]
        : -1;
  }
  int nextChar(off_t i = 1);
  Token currentToken() const noexcept { return currentToken_; }
  Token nextToken();
  void skipSpace();
  Token parseNumber();
  Token parseIdent();

  // syntax
  std::string parseUntilInitializer();
  std::string parseLine();
  Message parseMessage();
  AnalysisType parseAnalysisType();
  xzero::flow::SourceLocation parseLocation();
  std::string parseMessageText();
  void consume(Token t);
  std::string consumeIdent();

  void reportError(const std::string& msg);

  template<typename... Args>
  void reportError(const std::string& fmt, Args&&... args) {
    reportError(fmt::format(fmt, args...));
  }

 private:
  std::string filename_;
  std::string source_;
  Token currentToken_;
  xzero::flow::FilePos currentPos_;
};

class Tester : public xzero::flow::Runtime {
 public:
  Tester();

  bool testFile(const std::string& filename);
  bool testDirectory(const std::string& path);

 private:
  bool import(const std::string& name,
              const std::string& path,
              std::vector<xzero::flow::NativeCallback*>* builtins) override;
  void reportError(const std::string& msg);

  // handlers
  void flow_handler_true(xzero::flow::Params& args);
  void flow_handler(xzero::flow::Params& args);

  // functions
  void flow_sum(xzero::flow::Params& args);
  void flow_assert(xzero::flow::Params& args);

 private:
  uintmax_t errorCount_ = 0;
};

} // namespace flowtest

namespace fmt {
  template<>
  struct formatter<flowtest::AnalysisType> {
    using AnalysisType = flowtest::AnalysisType;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const AnalysisType& v, FormatContext &ctx) {
      switch (v) {
        case AnalysisType::TokenError:
          return format_to(ctx.begin(), "TokenError");
        case AnalysisType::SyntaxError:
          return format_to(ctx.begin(), "SyntaxError");
        case AnalysisType::TypeError:
          return format_to(ctx.begin(), "TypeError");
        case AnalysisType::Warning:
          return format_to(ctx.begin(), "Warning");
        case AnalysisType::LinkError:
          return format_to(ctx.begin(), "LinkError");
        default:
          return format_to(ctx.begin(), "{}", static_cast<unsigned>(v));
      }
    }
  };
}

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
          return format_to(ctx.begin(), "'----'");
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

#include <xzero-flow/flowtest-inl.h>
