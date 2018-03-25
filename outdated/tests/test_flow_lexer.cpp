// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/flow/FlowLexer.h>
#include <fstream>

using namespace x0;

int main(int argc, char *argv[]) {
  FlowLexer lexer;
  std::fstream input(argv[1]);
  if (!lexer.initialize(&input)) return 1;

  for (FlowToken t = lexer.token(); t != FlowToken::Eof;
       t = lexer.nextToken()) {
    SourceLocation location = lexer.location();
    std::string ts = lexer.tokenToString(t);
    std::string raw = lexer.locationContent();

    printf("[%04ld:%03ld.%03ld - %04ld:%03ld.%03ld] (%s): %s\n",
           location.begin.line, location.begin.column, location.begin.offset,
           location.end.line, location.end.column, location.end.offset,
           ts.c_str(), raw.c_str());
  }

  return true;
}
