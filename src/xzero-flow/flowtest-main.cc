#include <xzero-flow/flowtest.h>

#include <xzero-flow/FlowParser.h>
#include <xzero-flow/SourceLocation.h>
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

#include <fmt/format.h>

#include <iostream>
#include <vector>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

using namespace std;
using namespace xzero;
using namespace flowtest;
using flow::SourceLocation;

class Tester : public xzero::flow::Runtime {
 public:
  Tester();

  bool test(const std::string& path);
  bool testFile(const std::string& filename);
  bool testDirectory(const std::string& path);

 private:
  void compileFile(const std::string& filename, xzero::flow::diagnostics::Report* report);

  bool import(const std::string& name,
              const std::string& path,
              std::vector<xzero::flow::NativeCallback*>* builtins) override;

  void reportError(const std::string& msg);

  template<typename... Args>
  void reportError(const std::string& fmt, Args&&... args) {
    reportError(fmt::format(fmt, args...));
  }

  // handlers
  void flow_handler_true(xzero::flow::Params& args);
  void flow_handler(xzero::flow::Params& args);

  // functions
  void flow_sum(xzero::flow::Params& args);
  void flow_assert(xzero::flow::Params& args);

 private:
  xzero::flow::diagnostics::Report report_;
  uintmax_t errorCount_ = 0;

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
  fmt::print("{}\n", msg);
  errorCount_++;
}

bool Tester::test(const std::string& path) {
  fs::path p{path};
  if (fs::is_directory(p))
    return testDirectory(path);

  if (fs::is_regular_file(p))
    return testFile(path);
  return false;
}

bool Tester::testDirectory(const std::string& path) {
  int errorCount = 0;
  for (auto& dir: fs::recursive_directory_iterator(path)) {
    if (dir.path().extension() == ".flow") {
      report_.clear();
      if (!testFile(dir.path().string())) {
        report_.log();
        errorCount++;
      }
    }
  }

  return errorCount == 0;
}

static std::string readFile(const std::string& filename) {
  std::ifstream f(filename);
  std::stringstream sstr;
  sstr << f.rdbuf();
  return std::move(sstr.str());
}

bool Tester::testFile(const std::string& filename) {
  flow::diagnostics::Report actual;
  compileFile(filename, &actual);

  flow::diagnostics::Report expected;
  Parser p(filename, readFile(filename));
  std::error_code ec = p.parse(&expected);
  if (ec) {
    reportError("Parse Error({}): {}", ec.category().name(), ec.message());
    return false;
  }

  flow::diagnostics::DifferenceReport diff = flow::diagnostics::difference(actual, expected);
  for (const Message& m: diff.first)
    reportError("Missing: {}", m);
  for (const Message& m: diff.second)
    reportError("Superfluous: {}", m);

  if (actual != expected)
    return false;

  return true;
}

void Tester::compileFile(const std::string& filename, flow::diagnostics::Report* report) {
  fmt::print("testing {}\n", filename);

  constexpr bool optimize = true;
  flow::FlowParser parser(report,
                          this,
                          [this](auto x, auto y, auto z) { return import(x, y, z); });
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

  program->link(this, report);
}

int main(int argc, const char* argv[]) {
  Tester t;
  bool success = t.test(argv[1]);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
