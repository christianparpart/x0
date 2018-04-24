
namespace flowtest {

Lexer::Lexer(const std::string& filename, const std::string& contents)
    : filename_{filename},
      source_{contents},
      currentToken_{Token::Eof},
      currentPos_{},
      numberValue_{0},
      stringValue_{} {
  nextChar();
  size_t i = source_.find("\n# ----\n");
  if (i == std::string::npos) {
    currentToken_ = Token::InitializerMark;
    nextChar(i + 8);
  } else {
    nextToken();
  }
}

// ----------------------------------------------------------------------------
// lexic

int Lexer::nextChar(off_t i) {
  while (i > 0 && !eof()) {
    currentPos_.advance(currentChar());
    i--;
  }
  return currentChar();
}

bool Lexer::peekSequenceMatch(const std::string& sequence) const {
  if (currentOffset() + sequence.size() > source_.size())
    return false;

  for (size_t i = 0; i < sequence.size(); ++i)
    if (source_[currentOffset() + i] != sequence[i])
      return false;

  return true;
}

Token Lexer::nextToken() {
  skipSpace();
  switch (currentChar()) {
    case '#':
      if (peekSequenceMatch("# ----\n")) {
        nextChar(7);
        return currentToken_ = Token::InitializerMark;
      }
      nextChar();
      return currentToken_ = Token::Begin;
    case '.':
      if (peekChar() == '.') {
        nextChar(2);
        return currentToken_ = Token::DotDot;
      }
      break;
    case ':':
      nextChar();
      return currentToken_ = Token::Colon;
    case '[':
      nextChar();
      return currentToken_ = Token::BrOpen;
    case ']':
      nextChar();
      return currentToken_ = Token::BrClose;
    case '\n':
      nextChar();
      return currentToken_ = Token::LF;
    default:
      if (std::isdigit(currentChar())) {
        return currentToken_ = parseNumber();
      }
      if (std::isalpha(currentChar())) {
        return currentToken_ = parseIdent();
      }
  }
  throw LexerError{fmt::format("Unexpected character {} during tokenization.", (char)currentChar())};
}

Token Lexer::parseNumber() {
  numberValue_ = 0;

  while (std::isdigit(currentChar())) {
    numberValue_ *= 10;
    numberValue_ += currentChar() - '0';
    nextChar();
  }
  return Token::Number;
}

void Lexer::skipSpace() {
  for (;;) {
    switch (currentChar()) {
      case ' ':
      case '\t':
        nextChar();
        break;
      default:
        return;
    }
  }
}

} // namespace flowtest
