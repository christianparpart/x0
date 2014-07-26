// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/AST.h>
#include <flow/FlowParser.h>
#include <flow/vm/Runtime.h>
#include <string>
#include <memory>
#include <cstdio>
#include <unistd.h>

namespace flow {
class Instr;
namespace vm {
class Program;
} // vm
} // flow

using namespace flow;

class Flower : public flow::vm::Runtime {
 private:
  std::string filename_;
  std::unique_ptr<vm::Program> program_;
  size_t totalCases_;    // total number of cases ran
  size_t totalSuccess_;  // total number of succeed tests
  size_t totalFailed_;   // total number of failed tests

  bool dumpAST_;
  bool dumpIR_;
  bool dumpTarget_;

 public:
  Flower();
  ~Flower();

  virtual bool import(const std::string& name, const std::string& path,
                      std::vector<vm::NativeCallback*>* builtins);

  int optimizationLevel() { return 0; }
  void setOptimizationLevel(int val) {}

  void setDumpAST(bool value) { dumpAST_ = value; }
  void setDumpIR(bool value) { dumpIR_ = value; }
  void setDumpTarget(bool value) { dumpTarget_ = value; }

  int run(const char* filename, const char* handler);
  int runAll(const char* filename);
  void dump();

 private:
  bool onParseComplete(flow::Unit* unit);
  bool compile(flow::Unit* unit);

  // functions
  void flow_print(vm::Params& args);
  void flow_print_I(vm::Params& args);
  void flow_print_SI(vm::Params& args);
  void flow_print_IS(vm::Params& args);
  void flow_print_i(vm::Params& args);
  void flow_print_s(vm::Params& args);
  void flow_print_p(vm::Params& args);
  void flow_print_c(vm::Params& args);
  void flow_suspend(vm::Params& args);
  void flow_log(vm::Params& args);

  // handlers
  void flow_assert(vm::Params& args);
  void flow_finish(vm::Params& args);
  void flow_pass(vm::Params& args);
  void flow_assertFail(vm::Params& args);
  void flow_fail(vm::Params& args);
  void flow_error(vm::Params& args);
  void flow_getcwd(vm::Params& args);
  void flow_random(vm::Params& args);
  void flow_getenv(vm::Params& args);

  bool verify_numbers(Instr* call);
  void flow_numbers(vm::Params& args);
  void flow_names(vm::Params& args);

  // TODO: not ported yet
  //	void flow_mkbuf(vm::Params& args);
  //	void flow_getbuf(vm::Params& args);
};
