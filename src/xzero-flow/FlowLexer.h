// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/SourceLocation.h>
#include <xzero-flow/FlowToken.h>
#include <xzero-flow/LiteralType.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>

#include <utility>
#include <memory>
#include <fstream>
#include <cctype>
#include <list>

namespace xzero {
namespace flow {

//! \addtogroup Flow
//@{

class FlowLexer {
 public:
  FlowLexer();
  ~FlowLexer();

  void openLocalFile(const std::string& filename);

  void openStream(std::unique_ptr<std::istream>&& ifs,
                  const std::string& filename = "<stream>");

  void openString(const std::string& content);

  size_t depth() const { return contexts_.size(); }
  bool eof() const;

  // processing
  FlowToken nextToken();
  bool continueParseRegEx(char delim);

  // current parser state
  FlowToken token() const { return token_; }
  const SourceLocation& lastLocation() const { return lastLocation_; }
  const SourceLocation& location() const { return location_; }

  const std::string& filename() const { return location_.filename; }
  size_t line() const { return location_.end.line; }
  size_t column() const { return location_.end.column; }

  std::string stringValue() const { return stringValue_; }
  const IPAddress& ipValue() const { return ipValue_; }
  Cidr cidr() const { return Cidr(ipValue_, static_cast<size_t>(numberValue_)); }
  FlowNumber numberValue() const { return numberValue_; }

 private:
  struct Scope;

  Scope* enterScope(const std::string& filename);
  Scope* enterScope(std::unique_ptr<std::istream>&& ifs,
                    const std::string& filename);
  Scope* scope() const;
  void leaveScope();

  bool isHexChar() const;
  int currentChar() const;
  int peekChar();
  int nextChar(bool interscope = true);
  bool advanceUntil(char value);
  bool consume(char ch);
  bool consumeSpace();  // potentially enters new or leaves current context
  void processCommand(const std::string& line);

  FlowToken parseNumber(int base);
  FlowToken parseEnvVar();
  FlowToken parseRawString();
  FlowToken parseString(FlowToken result);
  FlowToken parseInterpolationFragment(bool start);
  FlowToken parseIdent();

  FlowToken continueParseIPv6(bool firstComplete);
  FlowToken continueCidr(size_t range);
  bool ipv6HexPart();
  bool ipv6HexSeq();
  bool ipv6HexDigit4();

 private:
  std::list<std::unique_ptr<Scope>> contexts_;

  int currentChar_;
  size_t ipv6HexDigits_;

  SourceLocation lastLocation_;
  SourceLocation location_;
  FlowToken token_;
  std::string stringValue_;
  IPAddress ipValue_;
  FlowNumber numberValue_;

  int interpolationDepth_;
};

struct FlowLexer::Scope {
  std::string filename;
  std::string basedir;
  std::unique_ptr<std::istream> stream;
  FilePos currPos;
  FilePos nextPos;
  int backupChar;  //!< backup of the outer scope's currentChar

  Scope();
  explicit Scope(std::unique_ptr<std::istream>&& input);

  void setStream(const std::string& filename,
                 std::unique_ptr<std::istream>&& istream);
};

// {{{ inlines
inline bool FlowLexer::isHexChar() const { return std::isxdigit(currentChar_); }

inline int FlowLexer::currentChar() const { return currentChar_; }

inline FlowLexer::Scope* FlowLexer::scope() const {
  return !contexts_.empty() ? contexts_.front().get() : nullptr;
}
// }}}

//!@}

}  // namespace flow
}  // namespace xzero
