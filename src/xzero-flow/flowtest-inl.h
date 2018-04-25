#include <xzero-flow/SourceLocation.h>
#include <xzero/Result.h>

namespace flowtest {

// ----------------------------------------------------------------------------
// lexic

Lexer::Lexer()
    : Lexer("", "") {
}

Lexer::Lexer(const std::string& filename, const std::string& contents)
    : filename_{filename},
      source_{contents},
      startOffset_{0},
      currentToken_{Token::Eof},
      currentPos_{},
      numberValue_{0},
      stringValue_{} {
  size_t i = source_.find("\n# ----\n");
  if (i != std::string::npos) {
    nextChar(i + 8);
    startOffset_ = i + 1;
    currentToken_ = Token::InitializerMark;
  } else {
    startOffset_ = source_.size();
    currentToken_ = Token::Eof;
  }
}

int Lexer::nextChar(off_t i) {
  while (i > 0 && !eof_()) {
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
    case -1:
      return currentToken_ = Token::Eof;
    case '#':
      // if (peekSequenceMatch("# ----\n")) {
      //   nextChar(7);
      //   return currentToken_ = Token::InitializerMark;
      // }
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
      if (currentToken_ == Token::Colon) {
        return currentToken_ = parseMessageText();
      }
      if (std::isdigit(currentChar())) {
        return currentToken_ = parseNumber();
      }
      if (std::isalpha(currentChar())) {
        return currentToken_ = parseIdent();
      }
  }
  throw LexerError{fmt::format("Unexpected character {} ({:x}) during tokenization.",
      currentChar() ? (char) currentChar() : '?', currentChar())};
}

Token Lexer::parseIdent() {
  stringValue_.clear();
  while (std::isalpha(currentChar())) {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }
  if (stringValue_ == "TokenError")
    return Token::TokenError;
  if (stringValue_ == "SyntaxError")
    return Token::SyntaxError;
  if (stringValue_ == "TypeError")
    return Token::TypeError;
  if (stringValue_ == "Warning")
    return Token::Warning;
  if (stringValue_ == "LinkError")
    return Token::LinkError;

  throw LexerError{fmt::format("Unexpected identifier '{}' during tokenization.",
                               stringValue_)};
}

Token Lexer::parseMessageText() {
  stringValue_.clear();
  while (!eof_() && currentChar() != '\n') {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }
  return Token::MessageText;
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

void Lexer::consume(Token t) {
  if (currentToken() != t)
    throw LexerError{fmt::format("Unexpected token {}. Expected {} instead.", currentToken(), t)};

  nextToken();
}

std::string Lexer::consumeText(Token t) {
  std::string result = stringValue();
  consume(t);
  return result;
}

std::string join(const std::initializer_list<Token>& tokens) {
  std::string s;
  for (Token t: tokens) {
    if (!s.empty())
      s += ", ";
    s += fmt::format("{}", t);
  }
  return s;
}

void Lexer::consumeOneOf(std::initializer_list<Token>&& tokens) {
  if (std::find(tokens.begin(), tokens.end(), currentToken()) == tokens.end())
    throw LexerError{fmt::format("Unexpected token {}. Expected on of {} instead.",
                                 currentToken(), join(tokens))};

  nextToken();
}

// ----------------------------------------------------------------------------
// parser


Parser::Parser(const std::string& filename, const std::string& source)
    : lexer_{filename, source} {
}

Result<ParseResult> Parser::parse() {
  lexer_.consume(Token::InitializerMark);
  ParseResult pr;
  pr.program = lexer_.getPrefixText();

  while (!lexer_.eof())
    pr.messages.push_back(parseMessage());

  return Success(std::move(pr));
}

Message Parser::parseMessage() {
  // Message   ::= '#' AnalysisType ':' Location MessageText (LF | EOF)
  // MessageText   ::= TEXT (LF INDENT TEXT)*
  // AnalysisType  ::= 'TokenError' | 'SyntaxError' | 'TypeError' | 'Warning' | 'LinkError'
  // Location      ::= '[' FilePos ['..' FilePos] ']'
  // FilePos       ::= Line ':' Column
  // Column        ::= NUMBER
  // Line          ::= NUMBER

  lexer_.consume(Token::Begin);
  AnalysisType type = parseAnalysisType();
  lexer_.consume(Token::Colon);
  xzero::flow::SourceLocation location = parseLocation();
  std::string text = lexer_.consumeText(Token::MessageText);
  lexer_.consumeOneOf({Token::LF, Token::Eof});

  std::vector<std::string> texts;
  texts.emplace_back(text);

  return Message{type, location, texts};
}

AnalysisType Parser::parseAnalysisType() {
  switch (lexer_.currentToken()) {
    case Token::TokenError:
      lexer_.nextToken();
      return AnalysisType::TokenError;
    case Token::SyntaxError:
      lexer_.nextToken();
      return AnalysisType::SyntaxError;
    case Token::TypeError:
      lexer_.nextToken();
      return AnalysisType::TypeError;
    case Token::Warning:
      lexer_.nextToken();
      return AnalysisType::Warning;
    case Token::LinkError:
      lexer_.nextToken();
      return AnalysisType::LinkError;
    default:
      throw SyntaxError{"Unexpected token. Expected AnalysisType instead."};
  }
}

xzero::flow::SourceLocation Parser::parseLocation() { // TODO
  // Location      ::= '[' FilePos ['..' FilePos] ']'
  // FilePos       ::= Line ':' Column
  // Column        ::= NUMBER
  // Line          ::= NUMBER

  return xzero::flow::SourceLocation();
}

} // namespace flowtest
