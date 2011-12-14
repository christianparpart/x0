/* <x0/test_flow_lexer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowLexer.h>
#include <fstream>

using namespace x0;

int main(int argc, char *argv[])
{
	FlowLexer lexer;
	std::fstream input(argv[1]);
	if (!lexer.initialize(&input))
		return 1;

	for (FlowToken t = lexer.token(); t != FlowToken::Eof; t = lexer.nextToken())
	{
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
