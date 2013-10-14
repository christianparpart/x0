/* <flow-tool/main.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2013 Christian Parpart <trapni@gmail.com>
 */

#include "Flower.h"
#include <x0/flow/Flow.h>
#include <x0/flow/FlowParser.h>
#include <x0/flow/FlowRunner.h>
#include <x0/flow/FlowBackend.h>
#include <fstream>
#include <memory>
#include <cstdio>
#include <getopt.h>
#include <unistd.h>

using namespace x0;

int usage(const char *program)
{
	printf(
		"usage: %s [-h] [-t] [-l] [-L] [-e entry_point] filename\n"
		"\n"
		"    -h      prints this help\n"
		"    -L      dumps LLVM IR of the compiled module\n"
		"    -l      Dump lexical output and exit\n"
		"    -e      entry point to start execution from. if not passed, nothing will be executed.\n"
		"    -On     set optimization level, with n ranging from 0 (no optimization) to 4 (maximum).\n"
		"    -t      enables unit-test mode\n"
		"\n",
		program
	);
	return 0;
}

int lexdump(const char* filename)
{
	FlowLexer lexer;
	std::fstream input(filename);
	if (!lexer.initialize(&input, filename))
		return 1;

	for (FlowToken t = lexer.token(); t != FlowToken::Eof; t = lexer.nextToken()) {
		SourceLocation location = lexer.location();
		std::string ts = lexer.tokenToString(t);
		std::string raw = lexer.locationContent();

		printf("[%04ld:%03ld.%03ld - %04ld:%03ld.%03ld] (%s): %s\n",
			location.begin.line, location.begin.column, location.begin.offset,
			location.end.line, location.end.column, location.end.offset,
			ts.c_str(), raw.c_str());
	}

	return 0;
}

int main(int argc, char *argv[])
{
	const char *handlerName = NULL;
	bool dumpIR = false;
	Flower flower;
	bool testMode = false;
	bool lexMode = false;
	int opt;
	int rv = 0;

	while ((opt = getopt(argc, argv, "tO:hLe:l")) != -1) { // {{{
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'L':
			dumpIR = true;
			break;
		case 'l':
			lexMode = true;
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
	} // }}}

	if (optind >= argc) {
		printf("Expected argument after options.\n");
		return EXIT_FAILURE;
	}

	while (argv[optind]) {
		const char *fileName = argv[optind];
		++optind;

		if (lexMode) {
			printf("%s:\n", fileName);
			return lexdump(fileName);
		}

		if (testMode) {
			printf("%s:\n", fileName);
			rv = flower.runAll(fileName);
		} else {
			flower.run(fileName, handlerName);
		}

		if (dumpIR)
			flower.dump();

		flower.clear();
	}

	FlowRunner::shutdown();

	return rv;
}
