/* <flow-tool/main.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2013 Christian Parpart <trapni@gmail.com>
 */

#include "Flower.h"
#include <x0/flow2/AST.h>
#include <x0/flow2/ASTPrinter.h>
#include <x0/flow2/FlowLexer.h>
#include <x0/flow2/FlowParser.h>
#include <x0/DebugLogger.h>
#include <fstream>
#include <memory>
#include <cstdio>
#include <getopt.h>
#include <unistd.h>

using namespace x0;

int usage(const char *program)
{
	printf(
		"usage: %s [-h] [-t] [-l] [-s] [-L] [-e entry_point] filename\n"
		"\n"
		"    -h      prints this help\n"
		"    -L      dumps LLVM IR of the compiled module\n"
		"    -l      Dump lexical output and exit\n"
		"    -s      Dump AST after parsing process\n"
		"    -e      entry point to start execution from. if not passed, nothing will be executed.\n"
		"    -On     set optimization level, with n ranging from 0 (no optimization) to 4 (maximum).\n"
		"    -t      enables unit-test mode\n"
		"\n",
		program
	);
	return 0;
}

int lexdump(const char* filename) // {{{
{
	FlowLexer lexer;
	if (!lexer.open(filename)) {
		perror("lexer.open");
		return 1;
	}

	for (FlowToken t = lexer.token(); t != FlowToken::Eof; t = lexer.nextToken()) {
		FlowLocation location = lexer.location();
		std::string raw = lexer.location().text();

		printf("[%04ld:%03ld.%04ld - %04ld:%03ld.%04ld] %10s %-30s %s\n",
			location.begin.line, location.begin.column, location.begin.offset,
			location.end.line, location.end.column, location.end.offset,
			t.c_str(), raw.c_str(),
			location.filename.c_str()
		);
	}

	return 0;
}
// }}}
int parsedump(const char* filename) // {{{
{
	FlowParser parser;

	if (!parser.open(filename)) {
		perror("parser.open");
		return 1;
	}

	parser.errorHandler = [&](const std::string& message) {
		fprintf(stderr, "Parser Error. %s\n", message.c_str());
	};

	parser.importHandler = [&](const std::string& moduleName, const std::string& path) -> bool {
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

int main(int argc, const char *argv[])
{
	const char *handlerName = NULL;
	Flower flower;
	bool testMode = false;
	bool lexMode = false;
	int opt;
	int rv = 0;

	DebugLogger::get().configure("XZERO_DEBUG");

	// {{{ args parsing
#if !defined(XZERO_NDEBUG)
	if (argc == 1) {
		static const char* debugArgs[] = { argv[0], "-s", "-L", "-e", "main", "./parse.flow", nullptr };
		argc = sizeof(debugArgs) / sizeof(*debugArgs) - 1;
		argv = debugArgs;
	}
#endif

	while ((opt = getopt(argc, (char**) argv, "tO:hLe:ls")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'L':
			flower.setDumpIR(true);
			break;
		case 'l':
			lexMode = true;
			break;
		case 's':
			flower.setDumpAST(true);
			break;
		case 't':
			testMode = true;
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
		const char *fileName = argv[optind];
		++optind;

		if (lexMode)
			return lexdump(fileName);

		if (testMode) {
			printf("%s:\n", fileName);
			rv = flower.runAll(fileName);
		} else {
			flower.run(fileName, handlerName);
		}

		flower.clear();
	}

	return rv;
}
