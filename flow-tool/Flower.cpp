/* <flow-tool/Flower.h>
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
#include <utility>
#include <cstdio>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

using namespace x0;

/*
 * TODO
 * - After parsing, traverse throught the AST and translate every assert()
 *   to also pass the string representation of the first argument as last argument.
 *
 */

void reportError(const char *category, const std::string& msg)
{
	printf("%s error: %s\n", category, msg.c_str());
}

Flower::Flower() :
	FlowBackend(),
	runner_(this)
{
	runner_.setErrorHandler(std::bind(&reportError, "vm", std::placeholders::_1));
	runner_.onParseComplete = std::bind(&Flower::onParseComplete, this, std::placeholders::_1);

	// properties
	registerProperty("cwd", FlowValue::STRING, &get_cwd);

	// functions
	registerFunction("getenv", FlowValue::STRING, &flow_getenv);
	registerFunction("mkbuf", FlowValue::BUFFER, &flow_mkbuf);
	registerFunction("getbuf", FlowValue::BUFFER, &flow_getbuf);

	// unit test aiding handlers
	registerHandler("error", &flow_error);
	registerHandler("finish", &flow_finish); // XXX rename to 'success'
	registerHandler("assert", &flow_assert);
	registerHandler("assert_fail", &flow_assertFail);

	registerHandler("fail", &flow_fail);
	registerHandler("pass", &flow_pass);
}

Flower::~Flower()
{
}

class CallCollector : public ASTVisitor // {{{
{
public:
	static void run(Unit* unit, std::list<Symbol*>& result)
	{
		CallCollector cc(result);
		cc.collect(unit);
	}

protected:
	std::list<Symbol*>& result_;
	int depth_;

	template<typename... Args>
	ssize_t printf(const char* fmt, const Args&... args)
	{
		for (int i = 0; i < depth_; ++i) std::printf("  ");
		return std::printf(fmt, args...);
	}

	void collect(ASTNode* n)
	{
		++depth_;
		n->accept(*this);
		--depth_;
	}

	CallCollector(std::list<Symbol*>& result) :
		result_(result),
		depth_(0)
	{
	}

	virtual void visit(Variable& var)
	{
		// no interest
	}

	virtual void visit(Function& func)
	{
		printf("Function: '%s'\n", func.name().c_str());
	}

	virtual void visit(Unit& unit)
	{
		printf("Unit: '%s'\n", unit.name().c_str());
		for (Symbol* sym: unit.members())
			collect(sym);
	}

	virtual void visit(UnaryExpr& expr)
	{
		collect(expr.subExpr());
	}

	virtual void visit(BinaryExpr& expr)
	{
		collect(expr.leftExpr());
		collect(expr.rightExpr());
	}

	virtual void visit(StringExpr& expr)
	{
	}

	virtual void visit(NumberExpr& expr)
	{
	}

	virtual void visit(BoolExpr& expr)
	{
	}

	virtual void visit(RegExpExpr& expr)
	{
	}

	virtual void visit(IPAddressExpr& expr)
	{
	}

	virtual void visit(VariableExpr& expr)
	{
	}

	virtual void visit(FunctionRefExpr& expr)
	{
		printf("FunctionRefExpr to '%s'\n", expr.function()->name().c_str());
	}

	virtual void visit(CastExpr& expr)
	{
	}

	virtual void visit(CallExpr& expr)
	{
	}

	virtual void visit(ListExpr& expr)
	{
	}

	virtual void visit(ExprStmt& stmt)
	{
	}

	virtual void visit(CompoundStmt& stmt)
	{
	}

	virtual void visit(CondStmt& stmt)
	{
	}
};
// }}}

std::string dumpSource(const std::string& filename, const FilePos& begin, const FilePos& end) // {{{
{
	std::string result;
	char* buf = nullptr;
	ssize_t size;
	ssize_t n;
	int fd;

	fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0)
		return std::string();

	size = end.offset - begin.offset;
	if (size <= 0)
		goto out;

	if (lseek(fd, begin.offset, SEEK_SET) < 0)
		goto out;

	buf = new char[size + 1];
	n = read(fd, buf, size); 
	if (n < 0)
		goto out;

	result = std::string(buf, n);

out:
	delete[] buf;
	close(fd);
	return result;
} // }}}

bool Flower::onParseComplete(Unit* unit)
{
	std::list<Symbol*> calls;
	printf("Flower.onParseComplete()\n");
//	CallCollector::run(unit, calls);

	for (auto& call: FlowCallIterator(unit)) {
		if (call->callee()->name() != "assert") continue;

		ListExpr* args = call->args();
		if (args->empty()) continue;

		Expr* arg = args->at(0);
		SourceLocation& sloc = arg->sourceLocation();
		std::string dump = sloc.dump();
		//std::string dump = dumpSource(filename_, sloc.begin, sloc.end);
		printf("call to: '%s' -> '%s'\n", call->callee()->name().c_str(), dump.c_str());
	}

	return true;
}

int Flower::runAll(const char *fileName)
{
	filename_ = fileName;
	if (!runner_.open(fileName)) {
		printf("Failed to load file: %s\n", fileName);
		return -1;
	}

	for (auto fn: runner_.getHandlerList()) {
		if (strncmp(fn->name().c_str(), "test_", 5) == 0) { // only consider handlers beginning with "test_"
			bool failed = runner_.invoke(fn);
			printf("Running %s... %s\n", fn->name().c_str(), failed ? "FAILED" : "OK");
		}
	}

	return 0;
}

int Flower::run(const char* fileName, const char* handlerName)
{
	if (!handlerName || !*handlerName) {
		printf("No handler specified.\n");
		return -1;
	}

	filename_ = fileName;
	if (!runner_.open(fileName)) {
		printf("Failed to load file: %s\n", fileName);
		return -1;
	}

	Function* fn = runner_.findHandler(handlerName);
	if (!fn) {
		printf("No handler with name '%s' found in unit '%s'.\n", handlerName, fileName);
		return -1;
	}

	if (handlerName) {
		bool handled = runner_.invoke(fn);
		return handled ? 0 : 1;
	}
	return 1;
}

void Flower::dump()
{
	runner_.dump();
}

void Flower::clear()
{
	runner_.clear();
}

void Flower::get_cwd(void *, x0::FlowParams& args, void *)
{
	static char buf[1024];

	args[0].set(getcwd(buf, sizeof(buf)) ? buf : strerror(errno));
}

void Flower::flow_mkbuf(void *, x0::FlowParams& args, void *)
{
	if (args.size() == 2 && args[1].isString())
		args[0].set(args[1].toString(), strlen(args[1].toString()));
	else
		args[0].set("", 0); // empty buffer
}

void Flower::flow_getbuf(void *, x0::FlowParams& args, void *)
{
	args[0].set("Some Long Buffer blabla", 9);
}

void Flower::flow_getenv(void *, x0::FlowParams& args, void *)
{
	args[0].set(getenv(args[1].toString()));
}

void Flower::flow_error(void *, x0::FlowParams& args, void *)
{
	if (args.size() == 2)
		printf("error. %s\n", args[1].toString());
	else
		printf("error\n");

	args[0].set(true);
}

void Flower::flow_finish(void *, x0::FlowParams& args, void *)
{
	args[0].set(true);
}

void Flower::flow_assert(void *, x0::FlowParams& args, void *)
{
	if (!args[1].toBool())
	{
		if (args.size() == 3 && args[2].isString())
			fprintf(stderr, "Assertion failed. %s\n", args[2].toString());
		else
			fprintf(stderr, "Assertion failed.\n");

		fflush(stderr);

		args[0].set(true);
	}
	else
	{
		//printf("assert ok (%d, %f)\n", args[1].type_, args[1].toNumber());
		args[0].set(false);
	}
}

void Flower::flow_fail(void *, x0::FlowParams& args, void *)
{
	args[0].set(true);
}

void Flower::flow_pass(void *, x0::FlowParams& args, void *)
{
	args[0].set(false);
}

void Flower::flow_assertFail(void *, x0::FlowParams& args, void *)
{
	if (args[1].toBool())
	{
		if (args.size() == 3 && args[2].isString())
			fprintf(stderr, "Assertion failed. %s\n", args[2].toString());
		else
			fprintf(stderr, "Assertion failed.\n");

		fflush(stderr);

		args[0].set(true);
	}
	else
	{
		//printf("assert ok (%d, %f)\n", args[1].type_, args[1].toNumber());
		args[0].set(false);
	}
}

bool Flower::printValue(const FlowValue& value, bool lf)
{
	switch (value.type_)
	{
		case FlowValue::BOOLEAN:
			printf(value.toBool() ? "true" : "false");
			break;
		case FlowValue::NUMBER:
			printf("%lld", value.toNumber());
			break;
		case FlowValue::STRING:
			printf("%s", value.toString());
			break;
		case FlowValue::BUFFER: {
			long long length = value.toNumber();
			const char *p = value.toString();
			std::string data(p, p + length);
			printf("buffer.len  : %lld\n", length);
			printf("buffer.data : %s\n", data.c_str());
			break;
		}
		case FlowValue::ARRAY: {
			const FlowArray& p = value.toArray();
			printf("(");
			for (size_t k = 0, ke = p.size(); k != ke; ++k) {
				if (k) printf(", ");
				printValue(p[k], false);
			}
			printf(")");

			break;
		}
		default:
			return false;
	}

	if (lf)
		printf("\n");

	return true;
}
