// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/FlowParser.h>
#include <xzero-flow/IRGenerator.h>
#include <xzero-flow/NativeCallback.h>
#include <xzero-flow/Params.h>
#include <xzero-flow/TargetCodeGenerator.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/PassManager.h>
#include <xzero-flow/transform/EmptyBlockElimination.h>
#include <xzero-flow/transform/InstructionElimination.h>
#include <xzero-flow/transform/MergeBlockPass.h>
#include <xzero-flow/transform/UnusedBlockPass.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/Runtime.h>

#include <xzero/io/FileUtil.h>
#include <xzero/logging.h>
#include <fmt/format.h>

#include <iostream>
#include <experimental/filesystem>

using namespace std;
using namespace xzero;

namespace fs = std::experimental::filesystem;

// lexer, lexic, lexical
// parser, syntax, syntactical
// type, semantic, semantical
// warning
//
// LexicError
// SyntaxError
// SemanticError

class Tester : public flow::Runtime {
 public:
  Tester();

  bool testFile(const std::string& filename);
  bool testDirectory(const std::string& path);

 private:
  bool import(const std::string& name,
              const std::string& path,
              std::vector<flow::NativeCallback*>* builtins) override;
  void reportError(const std::string& msg);

  // handlers
  void flow_handler_true(flow::Params& args);
  void flow_handler(flow::Params& args);

  // functions
  void flow_sum(flow::Params& args);
  void flow_assert(flow::Params& args);

 private:
  int errorCount_ = 0;
};

Tester::Tester() {
  registerHandler("handler.true")
      .bind(&Tester::flow_handler_true, this);

  registerHandler("handler")
      .bind(&Tester::flow_handler, this)
      .param<flow::FlowNumber>("result");

  registerFunction("sum", flow::LiteralType::Number)
      .bind(&Tester::flow_sum, this)
      .param<flow::FlowNumber>("x")
      .param<flow::FlowNumber>("y");

  registerFunction("assert", flow::LiteralType::Number)
      .bind(&Tester::flow_assert, this)
      .param<flow::FlowNumber>("condition")
      .param<flow::FlowString>("description", "");
}

void Tester::flow_handler_true(flow::Params& args) {
  args.setResult(true);
}

void Tester::flow_handler(flow::Params& args) {
  args.setResult(args.getBool(1));
}

void Tester::flow_sum(flow::Params& args) {
  const flow::FlowNumber x = args.getInt(1);
  const flow::FlowNumber y = args.getInt(2);
  args.setResult(x + y);
}

void Tester::flow_assert(flow::Params& args) {
  const bool condition = args.getBool(1);
  const std::string description = args.getString(2);

  if (!condition) {
    if (description.empty())
      reportError("Assertion failed.");
    else
      reportError(fmt::format("Assertion failed ({}).", description));
  }
}

bool Tester::import(
    const std::string& name,
    const std::string& path,
    std::vector<flow::NativeCallback*>* builtins) {
  return true;
}

void Tester::reportError(const std::string& msg) {
  fmt::print("Configuration file error. {}\n", msg);
  errorCount_++;
}

bool Tester::testDirectory(const std::string& p) {
  for (auto& dir: fs::recursive_directory_iterator(p))
    if (dir.path().extension() == ".flow")
      if (!testFile(dir.path().string()))
        return false;

  return true;
}

bool Tester::testFile(const std::string& filename) {
  fmt::print("testing: {}\n", filename);

  constexpr bool optimize = true;

  flow::FlowParser parser(this,
                          [this](auto x, auto y, auto z) { return import(x, y, z); },
                          [this](const std::string& msg) { reportError(msg); });
  parser.openStream(std::make_unique<std::ifstream>(filename), filename);
  std::unique_ptr<flow::UnitSym> unit = parser.parse();

  flow::IRGenerator irgen([this] (const std::string& msg) { reportError(msg); },
                          {"main"});
  std::shared_ptr<flow::IRProgram> programIR = irgen.generate(unit.get());

  if (optimize) {
    flow::PassManager pm;
    pm.registerPass(std::make_unique<flow::UnusedBlockPass>());
    pm.registerPass(std::make_unique<flow::MergeBlockPass>());
    pm.registerPass(std::make_unique<flow::EmptyBlockElimination>());
    pm.registerPass(std::make_unique<flow::InstructionElimination>());

    pm.run(programIR.get());
  }

  std::unique_ptr<flow::Program> program =
      flow::TargetCodeGenerator().generate(programIR.get());

  program->link(this);

  return errorCount_ == 0;
}

int main(int argc, const char* argv[]) {
  Tester t;
  bool success = t.testDirectory(argv[1]);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
