/* <x0/flow_test.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

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

void reportError(const char *category, const std::string& msg)
{
	printf("%s error: %s\n", category, msg.c_str());
}

class Flower : public FlowBackend // {{{
{
private:
	FlowRunner runner_;

public:
	Flower();
	~Flower();

	int optimizationLevel() { return runner_.optimizationLevel(); }
	void setOptimizationLevel(int val) { runner_.setOptimizationLevel(val); }

	int run(const char *filename, const char *handler);
	int runAll(const char *filename);
	void dump();
	void clear();

	// {{{ backend API
private:
	static void get_cwd(void *, x0::FlowParams& args, void *)
	{
		static char buf[1024];

		args[0].set(getcwd(buf, sizeof(buf)) ? buf : strerror(errno));
	}

	static void flow_mkbuf(void *, x0::FlowParams& args, void *)
	{
		if (args.size() == 2 && args[1].isString())
			args[0].set(args[1].toString(), strlen(args[1].toString()));
		else
			args[0].set("", 0); // empty buffer
	}

	static void flow_getbuf(void *, x0::FlowParams& args, void *)
	{
		args[0].set("Some Long Buffer blabla", 9);
	}

	static void flow_getenv(void *, x0::FlowParams& args, void *)
	{
		args[0].set(getenv(args[1].toString()));
	}

	static bool printValue(const FlowValue& value, bool lf)
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

	static void flow_error(void *, x0::FlowParams& args, void *)
	{
		if (args.size() == 2)
			printf("error. %s\n", args[1].toString());
		else
			printf("error\n");

		args[0].set(true);
	}

	static void flow_finish(void *, x0::FlowParams& args, void *)
	{
		args[0].set(true);
	}

	static void flow_assert(void *, x0::FlowParams& args, void *)
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

	static void flow_fail(void *, x0::FlowParams& args, void *)
	{
		args[0].set(true);
	}

	static void flow_pass(void *, x0::FlowParams& args, void *)
	{
		args[0].set(false);
	}

	static void flow_assertFail(void *, x0::FlowParams& args, void *)
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
	// }}}
}; // }}}

// {{{ Flower impl
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

int usage(const char *program)
{
	printf(
		"usage: %s [-h] [-t] [-L] [-e entry_point] filename\n"
		"\n"
		"    -h      prints this help\n"
		"    -L      dumps LLVM IR of the compiled module\n"
		"    -e      entry point to start execution from. if not passed, nothing will be executed.\n"
		"    -On     set optimization level, with n ranging from 0 (no optimization) to 4 (maximum).\n"
		"    -t      enables unit-test mode\n"
		"\n",
		program
	);
	return 0;
}
// }}}

int main(int argc, char *argv[])
{
	const char *handlerName = NULL;
	bool dumpIR = false;
	Flower flower;
	bool testMode = false;

	// {{{ parse args
	int opt;
	while ((opt = getopt(argc, argv, "tO:hLe:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'L':
			dumpIR = true;
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

		if (testMode) {
			printf("%s:\n", fileName);
			flower.runAll(fileName);
		} else {
			flower.run(fileName, handlerName);
		}

		if (dumpIR)
			flower.dump();

		flower.clear();
	}

	FlowRunner::shutdown();

	return 0;
}
