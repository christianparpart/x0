#include <x0/flow2/AST.h>
#include <x0/flow2/FlowLexer.h>
#include <x0/flow2/FlowParser.h>
#include <x0/DebugLogger.h>

using namespace x0;

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
	parser.parse();
	return 0;
}
// }}}

int main(int argc, const char* argv[]) {
	DebugLogger::get().configure("XZERO_DEBUG");

	//lexdump(argc == 2 ? argv[1] : "lex.flow");
	parsedump(argc == 2 ? argv[1] : "parse.flow");
	return 0;
}
