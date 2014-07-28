// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "Flower.h"
#include <flow/AST.h>
#include <flow/ASTPrinter.h>
#include <flow/FlowLexer.h>
#include <flow/FlowParser.h>
#include <base/Trie.h>
#include <base/PrefixTree.h>
#include <base/SuffixTree.h>
#include <base/DebugLogger.h>
#include <fstream>
#include <memory>
#include <cstdio>
#include <getopt.h>
#include <unistd.h>

using namespace flow;

int usage(const char* program) {
  printf(
      "usage: %s [-h] [-t] [-l] [-s] [-L] [-A] [-I] [-T] [-e entry_point] "
      "filename\n"
      "\n"
      "    -h      prints this help\n"
      "    -L      Dump lexical output and exit\n"
      "    -A      Dump AST after parsing process\n"
      "    -I      dumps IR of the compiled module\n"
      "    -T      Dump target program code\n"
      "    -e      entry point to start execution from. if not passed, nothing "
      "will be executed.\n"
      "    -On     set optimization level, with n ranging from 0 (no "
      "optimization) to 4 (maximum).\n"
      "    -t      enables unit-test mode\n"
      "\n",
      program);
  return 0;
}

int lexdump(const char* filename)  // {{{
{
  FlowLexer lexer;
  if (!lexer.open(filename)) {
    perror("lexer.open");
    return 1;
  }

  for (FlowToken t = lexer.token(); t != FlowToken::Eof;
       t = lexer.nextToken()) {
    FlowLocation location = lexer.location();
    std::string raw = lexer.location().text();

    printf("[%04zu:%03zu.%04zu - %04zu:%03zu.%04zu] %10s %-30s %s\n",
           location.begin.line, location.begin.column, location.begin.offset,
           location.end.line, location.end.column, location.end.offset,
           t.c_str(), raw.c_str(), location.filename.c_str());
  }

  return 0;
}
// }}}
int parsedump(const char* filename)  // {{{
{
  FlowParser parser(nullptr);

  if (!parser.open(filename)) {
    perror("parser.open");
    return 1;
  }

  parser.errorHandler = [&](const std::string& message) {
    fprintf(stderr, "Parser Error. %s\n", message.c_str());
  };

  parser.importHandler = [&](const std::string& moduleName,
                             const std::string& path,
                             std::vector<vm::NativeCallback*>*)
                             ->bool {
    printf("importHandler: '%s' from '%s'\n", moduleName.c_str(), path.c_str());
    return true;
  };

  std::unique_ptr<Unit> unit = parser.parse();
  if (!unit) {
    printf("parsing failed\n");
    return 1;
  }

  ASTPrinter::print(unit.get());

  return 0;
}
// }}}

int main(int argc, const char* argv[]) {
  const char* handlerName = NULL;
  Flower flower;
  bool testMode = false;
  bool lexMode = false;
  int opt;
  int rv = 0;

  DebugLogger::get().configure("XZERO_DEBUG");

// {{{ args parsing
#if !defined(NDEBUG)
  if (argc == 1) {
    static const char* debugArgs[] = {argv[0], "-A",   "-I",           "-T",
                                      "-e",    "main", "./parse.flow", nullptr};
    argc = sizeof(debugArgs) / sizeof(*debugArgs) - 1;
    argv = debugArgs;
  }
#endif

  while ((opt = getopt(argc, (char**)argv, "tO:hAILTe:")) != -1) {
    switch (opt) {
      case 'h':
        usage(argv[0]);
        return 0;
      case 't':
        testMode = true;
        break;
      case 'L':
        lexMode = true;
        break;
      case 'A':
        flower.setDumpAST(true);
        break;
      case 'I':
        flower.setDumpIR(true);
        break;
      case 'T':
        flower.setDumpTarget(true);
        break;
      case 'O':
        flower.setOptimizationLevel(atoi(optarg));
        break;
      case 'e':
        handlerName = optarg;
        break;
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if (optind >= argc) {
    printf("Expected argument after options.\n");
    return EXIT_FAILURE;
  }
  // }}}

  while (argv[optind]) {
    const char* fileName = argv[optind];
    ++optind;

    if (lexMode) return lexdump(fileName);

    if (testMode) {
      printf("%s:\n", fileName);
      rv = flower.runAll(fileName);
    } else {
      flower.run(fileName, handlerName);
    }
  }

  return rv;
}
