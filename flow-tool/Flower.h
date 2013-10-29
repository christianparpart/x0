#pragma once
/* <flow-tool/Flower.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.trapni.de/projects/x0
 *
 * (c) 2010-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow2/AST.h>
#include <x0/flow2/FlowParser.h>
#include <x0/flow2/FlowBackend.h>
#include <x0/flow2/FlowMachine.h>
#include <string>
#include <memory>
#include <cstdio>
#include <unistd.h>

using namespace x0;

class Flower : public x0::FlowBackend
{
private:
	std::string filename_;
	x0::FlowMachine vm_;
	size_t totalCases_;		// total number of cases ran
	size_t totalSuccess_;	// total number of succeed tests
	size_t totalFailed_;	// total number of failed tests

	bool dumpAST_;
	bool dumpIR_;

public:
	Flower();
	~Flower();

	virtual bool import(const std::string& name, const std::string& path);

	int optimizationLevel() { return 0; }  // TODO runner_.optimizationLevel(); }
	void setOptimizationLevel(int val) { } // TODO runner_.setOptimizationLevel(val); }

	void setDumpAST(bool value) { dumpAST_ = value; }
	void setDumpIR(bool value) { dumpIR_ = value; }

	int run(const char *filename, const char *handler);
	int runAll(const char *filename);
	void dump();
	void clear();

private:
	bool onParseComplete(x0::Unit* unit);

	// functions
	void flow_print(FlowParams& args);

	// handlers
	void flow_assert(FlowParams& args);

	// TODO: not ported yet
//	static void get_cwd(void *, x0::FlowParams& args, void *);
//	static void flow_mkbuf(void *, x0::FlowParams& args, void *);
//	static void flow_getbuf(void *, x0::FlowParams& args, void *);
//	static void flow_getenv(void *, x0::FlowParams& args, void *);
//	static void flow_error(void *, x0::FlowParams& args, void *);
//	static void flow_finish(void *, x0::FlowParams& args, void *);
//	static void flow_fail(void *, x0::FlowParams& args, void *);
//	static void flow_pass(void *, x0::FlowParams& args, void *);
//	static void flow_assertFail(void *, x0::FlowParams& args, void *);

//	static bool printValue(const x0::FlowValue& value, bool lf);
};
