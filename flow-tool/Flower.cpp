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
#include <cstdio>
#include <unistd.h>

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

int Flower::runAll(const char *fileName)
{
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
