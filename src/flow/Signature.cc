// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/Signature.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace xzero::flow {

Signature::Signature()
    : name_(),
      returnType_(LiteralType::Void),
      args_() {
}

Signature::Signature(const std::string& signature)
    : name_(), returnType_(LiteralType::Void), args_() {
  // signature  ::= NAME [ '(' args ')' returnType
  // args       ::= type*
  // returnType ::= primitive | 'V'
  // type       ::= array | assocArray | primitive
  // array      ::= '[' primitive
  // assocArray ::= '>' primitive primitive
  // primitive  ::= 'B' | 'I' | 'S' | 'P' | 'C' | 'R' | 'H'

  enum class State {
    END = 0,
    Name = 1,
    ArgsBegin = 2,
    Args = 3,
    ReturnType = 4
  };

  const char* i = signature.data();
  const char* e = signature.data() + signature.size();
  State state = State::Name;

  while (i != e) {
    switch (state) {
      case State::Name:
        if (*i == '(') {
          state = State::ArgsBegin;
        }
        ++i;
        break;
      case State::ArgsBegin:
        name_ = std::string(signature.data(), i - signature.data() - 1);
        state = State::Args;
        break;
      case State::Args:
        if (*i == ')') {
          state = State::ReturnType;
        } else {
          args_.push_back(typeSignature(*i));
        }
        ++i;
        break;
      case State::ReturnType:
        returnType_ = typeSignature(*i);
        state = State::END;
        ++i;
        break;
      case State::END:
        fprintf(stderr, "Garbage at end of signature string. %s\n", i);
        i = e;
        break;
    }
  }

  if (state != State::END) {
    fprintf(stderr, "Premature end of signature string. %s\n",
            signature.c_str());
  }
}

std::string Signature::to_s() const {
  std::string result = name_;
  result += "(";
  for (LiteralType t : args_) result += signatureType(t);
  result += ")";
  result += signatureType(returnType_);
  return result;
}

LiteralType typeSignature(char ch) {
  switch (ch) {
    case 'V':
      return LiteralType::Void;
    case 'B':
      return LiteralType::Boolean;
    case 'I':
      return LiteralType::Number;
    case 'S':
      return LiteralType::String;
    case 'P':
      return LiteralType::IPAddress;
    case 'C':
      return LiteralType::Cidr;
    case 'R':
      return LiteralType::RegExp;
    case 'H':
      return LiteralType::Handler;
    case 's':
      return LiteralType::StringArray;
    case 'i':
      return LiteralType::IntArray;
    case 'p':
      return LiteralType::IPAddrArray;
    case 'c':
      return LiteralType::CidrArray;
    default:
      abort();
      return LiteralType::Void;
  }
}

char signatureType(LiteralType t) {
  switch (t) {
    case LiteralType::Void:
      return 'V';
    case LiteralType::Boolean:
      return 'B';
    case LiteralType::Number:
      return 'I';
    case LiteralType::String:
      return 'S';
    case LiteralType::IPAddress:
      return 'P';
    case LiteralType::Cidr:
      return 'C';
    case LiteralType::RegExp:
      return 'R';
    case LiteralType::Handler:
      return 'H';
    case LiteralType::StringArray:
      return 's';
    case LiteralType::IntArray:
      return 'i';
    case LiteralType::IPAddrArray:
      return 'p';
    case LiteralType::CidrArray:
      return 'c';
    default:
      abort();
      return '?';
  }
}

}  // namespace xzero::flow
