// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <iostream>

#include <xzero-flow/FlowParser.h>
#include <xzero-flow/IRGenerator.h>
#include <xzero-flow/NativeCallback.h>
#include <xzero-flow/TargetCodeGenerator.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/PassManager.h>
#include <xzero-flow/transform/EmptyBlockElimination.h>
#include <xzero-flow/transform/InstructionElimination.h>
#include <xzero-flow/transform/MergeBlockPass.h>
#include <xzero-flow/transform/UnusedBlockPass.h>
#include <xzero-flow/vm/Runtime.h>
#include <xzero-flow/vm/Program.h>

#include <xzero/io/FileUtil.h>
#include <xzero/logging.h>
#include <fmt/format.h>

using namespace std;
using namespace xzero;

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
  bool testFile(const std::string& filename);

 private:
  bool import(const std::string& name,
              const std::string& path,
              std::vector<flow::NativeCallback*>* builtins) override;
  void reportError(const std::string& msg);

 private:
  int errorCount_ = 0;
};

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

bool Tester::testFile(const std::string& filename) {
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
  bool success = t.testFile(argv[1]);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
