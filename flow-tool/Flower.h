#pragma once
/* <flow-tool/Flower.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/AST.h>
#include <x0/flow/FlowParser.h>
#include <x0/flow/vm/Runtime.h>
#include <string>
#include <memory>
#include <cstdio>
#include <unistd.h>

namespace x0 {
    namespace FlowVM {
        class Program;
    }
}

using namespace x0;

class Flower : public x0::FlowVM::Runtime
{
private:
	std::string filename_;
    std::unique_ptr<FlowVM::Program> program_;
	size_t totalCases_;		// total number of cases ran
	size_t totalSuccess_;	// total number of succeed tests
	size_t totalFailed_;	// total number of failed tests

	bool dumpAST_;
	bool dumpIR_;

public:
	Flower();
	~Flower();

	virtual bool import(const std::string& name, const std::string& path);

	int optimizationLevel() { return 0; }
	void setOptimizationLevel(int val) { }

	void setDumpAST(bool value) { dumpAST_ = value; }
	void setDumpIR(bool value) { dumpIR_ = value; }

	int run(const char *filename, const char *handler);
	int runAll(const char *filename);
	void dump();

private:
	bool onParseComplete(x0::Unit* unit);

	// functions
	void flow_print(FlowVM::Params& args);
    void flow_log(FlowVM::Params& args);

	// handlers
	void flow_assert(FlowVM::Params& args);
    void flow_finish(FlowVM::Params& args);
    void flow_pass(FlowVM::Params& args);
    void flow_assertFail(FlowVM::Params& args);
    void flow_fail(FlowVM::Params& args);
    void flow_error(FlowVM::Params& args);
    void flow_getcwd(FlowVM::Params& args);
    void flow_getenv(FlowVM::Params& args);

    void flow_numbers(FlowVM::Params& args);
    void flow_names(FlowVM::Params& args);

	// TODO: not ported yet
//	void flow_mkbuf(FlowVM::Params& args);
//	void flow_getbuf(FlowVM::Params& args);

//	bool printValue(const x0::FlowValue& value, bool lf);
};
