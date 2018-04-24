
namespace flowtest {

Lexer::Lexer(const std::string& filename, const std::string& contents)
    : filename_{filename},
      source_{contents},
      currentToken_{Token::Eof},
      currentPos_{} {
  nextChar();
  nextToken();
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

Token Lexer::nextToken() {
  skipSpace();
  switch (currentChar()) {
    case '#':
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
    case '-':
      if (peekChar(1) == '-' &&
          peekChar(2) == '-' &&
          peekChar(3) == '-') {
        nextChar(4);
        return currentToken_ = Token::InitializerMark;
      }
      break;
    default:
      if (std::isdigit(currentChar())) {
        return currentToken_ = parseNumber();
      }
      if (std::isalpha(currentChar())) {
        return currentToken_ = parseIdent();
      }
  }
  reportError("Unexpected character {} during tokenization.", (char)currentChar());
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
