/* <flow-tool/Flower.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2014 Christian Parpart <trapni@gmail.com>
 */
#include "Flower.h"
#include <x0/flow/AST.h>
#include <x0/flow/ASTPrinter.h>
#include <x0/flow/FlowParser.h>
#include <x0/flow/FlowAssemblyBuilder.h>
#include <x0/flow/vm/Runtime.h>
#include <x0/flow/vm/NativeCallback.h>
#include <x0/flow/vm/Runner.h>
#include <x0/flow/FlowCallVisitor.h>
#include <fstream>
#include <memory>
#include <utility>
#include <cstdio>
#include <cassert>

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
	Runtime(),
    filename_(),
    program_(nullptr),
	totalCases_(0),
	totalSuccess_(0),
	totalFailed_(0),
	dumpAST_(false),
	dumpIR_(false)
{
//	runner_.setErrorHandler(std::bind(&reportError, "vm", std::placeholders::_1));
//	runner_.onParseComplete = std::bind(&Flower::onParseComplete, this, std::placeholders::_1);

	// properties
//	registerProperty("cwd", FlowValue::STRING, &get_cwd);

	// functions
//	registerFunction("getenv", FlowValue::STRING, &flow_getenv);
//	registerFunction("mkbuf", FlowValue::BUFFER, &flow_mkbuf);
//	registerFunction("getbuf", FlowValue::BUFFER, &flow_getbuf);

	registerFunction("__print", FlowType::Void)
		.params(FlowType::String)
		.bind(&Flower::flow_print);

    registerFunction("log", FlowType::Void)
        .param<FlowString>("message", "<whaaaaat!>")
        .param<FlowNumber>("severity", 42)
        .bind(&Flower::flow_log)
        ;

	// unit test aiding handlers
//	registerHandler("error", &flow_error);
//	registerHandler("finish", &flow_finish); // XXX rename to 'success'

	registerHandler("assert")
		.param<bool>("condition")
        .param<FlowString>("description", "")
		.bind(&Flower::flow_assert);

//	registerHandler("assert_fail", &flow_assertFail);

//	registerHandler("fail", &flow_fail);
//	registerHandler("pass", &flow_pass);
}

Flower::~Flower()
{
}

bool Flower::import(const std::string& name, const std::string& path)
{
	return false;
}

bool Flower::onParseComplete(Unit* unit)
{
    FlowCallVisitor callv(unit);

    for (auto& call: callv.handlerCalls()) {
        if (call->callee()->name() != "assert" && call->callee()->name() != "assert_fail")
            continue;

        ParamList& args = call->args();
        assert(args.size() == 2);

        // add a string argument that equals the expression's source code
        Expr* arg = args.values()[0];
        std::string source = arg->location().text();
        args.replace("description", std::make_unique<StringExpr>(source, FlowLocation()));
    }

	return true;
}

int Flower::runAll(const char *fileName)
{
	filename_ = fileName;
//	if (!runner_.open(fileName)) {
//		printf("Failed to load file: %s\n", fileName);
//		return -1;
//	}

//	for (auto fn: runner_.getHandlerList()) {
//		if (strncmp(fn->name().c_str(), "test_", 5) == 0) { // only consider handlers beginning with "test_"
//			printf("[ -------- ] Testing %s\n", fn->name().c_str());
//			totalCases_++;
//			bool failed = runner_.invoke(fn);
//			if (failed) totalFailed_++;
//			printf("[ -------- ] %s\n\n", failed ? "FAILED" : "OK");
//		}
//	}

	printf("[ ======== ] %zu tests from %zu cases ran\n", totalSuccess_ + totalFailed_, totalCases_);
	if (totalSuccess_)
		printf("[  PASSED  ] %zu tests\n", totalSuccess_);
	if (totalFailed_)
		printf("[  FAILED  ] %zu tests\n", totalFailed_);

	return totalFailed_;
}

int Flower::run(const char* fileName, const char* handlerName)
{
	if (!handlerName || !*handlerName) {
		printf("No handler specified.\n");
		return -1;
	}

	filename_ = fileName;

//	if (!runner_.open(fileName)) {
//		printf("Failed to load file: %s\n", fileName);
//		return -1;
//	}

	FlowParser parser(this);

	parser.importHandler = [&](const std::string& name, const std::string& basedir) -> bool {
		fprintf(stderr, "parser.importHandler('%s', '%s')\n", name.c_str(), basedir.c_str());
		return false;
	};

	if (!parser.open(fileName)) {
		fprintf(stderr, "Failed to open file: %s\n", fileName);
		return -1;
	}

	std::unique_ptr<Unit> unit = parser.parse();
	if (!unit) {
		fprintf(stderr, "Failed to parse file: %s\n", fileName);
		return -1;
	}

    onParseComplete(unit.get());

	if (dumpAST_)
		ASTPrinter::print(unit.get());

//	if (!vm_.compile(unit.get())) {
//		fprintf(stderr, "Failed to compile file: %s\n", fileName);
//		return -1;
//	}

	Handler* handlerSym = unit->findHandler(handlerName);
	if (!handlerSym) {
		fprintf(stderr, "No handler with name '%s' found in unit '%s'.\n", handlerName, fileName);
		return -1;
	}

    program_ = FlowAssemblyBuilder::compile(unit.get());
    if (!program_) {
        fprintf(stderr, "Code generation failed. Aborting.\n");
        return -1;
    }

    if (!program_->link(this)) {
        fprintf(stderr, "Program linking failed. Aborting.\n");
        return -1;
    }

    if (dumpIR_) {
        printf("Dumping IR ...\n");
        program_->dump();
    }

    FlowVM::Handler* handler = program_->findHandler(handlerName);
    assert(handler != nullptr);

    printf("Running handler %s ...\n", handlerName);
    return handler->run(nullptr /*userdata*/ );
    //return false;
}

void Flower::dump()
{
    program_->dump();
}

void Flower::clear()
{
	//runner_.clear();
}

void Flower::flow_print(FlowVM::Params& args)
{
	printf("%s\n", args.get<FlowString*>(1)->str().c_str());
}

void Flower::flow_log(FlowVM::Params& args)
{
    FlowString* message = args.get<FlowString*>(1);
    FlowNumber severity = args.get<FlowNumber>(2);

    printf("<%lu> %s\n", severity, message->str().c_str());
}

void Flower::flow_assert(FlowVM::Params& args)
{
	const FlowString* sourceValue = args.get<FlowString*>(2);

    if (!args.get<bool>(1)) {
		printf("[   FAILED ] %s\n", sourceValue->str().c_str());
		args.setResult(true);
	} else {
		printf("[       OK ] %s\n", sourceValue->str().c_str());
		++totalSuccess_;
		args.setResult(false);
	}
}

#if 0
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
#endif
