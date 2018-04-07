// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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

#include <xzero/io/FileUtil.h>
#include <xzero/logging.h>
#include <fmt/format.h>

#include <iostream>
#include <vector>
#include <experimental/filesystem>

using namespace std;
using namespace xzero;

namespace fs = std::experimental::filesystem;

enum class AnalysisType { TokenError, SyntaxError, TypeError, Warning, LinkError };

/*
  TestProgram   ::= FlowProgram [Initializer TestMessage*]
  FlowProgram   ::= <flow program code until Initializer>

  Initializer   ::= '#' '----' LF
  TestMessage   ::= '#' AnalysisType ':' Location MessageText LF
  AnalysisType  ::= 'TokenError' | 'SyntaxError' | 'TypeError' | 'Warning' | 'LinkError'

  Location      ::= '[' FilePos ['..' FilePos] ']'
  FilePos       ::= Line ':' Column
  Column        ::= NUMBER
  Line          ::= NUMBER

  MessageText   ::= TEXT (LF INDENT TEXT)*

  NUMBER        ::= ('0'..'9')+
  TEXT          ::= <until LF>
  LF            ::= '\n' | '\r\n'
  INDENT        ::= (' ' | '\t')+
*/

struct TestMessage {
  AnalysisType type;
  flow::SourceLocation sourceLocation;
  std::vector<std::string> texts;
};

/**
 * Parses the input @p contents and splits it into a flow program and a vector
 * of analysis TestMessage.
 */
class TestParser {
 public:
  TestParser(const std::string& filename, std::string& contents);

  bool parse(std::string* programText, std::vector<TestMessage>* messages);

 public: // accessors
  std::string program() const;

 private:
  std::string parseUntilInitializer();
  std::string parseLine();
  TestMessage parseMessage();

 private:
  std::string filename_;
  std::string contents_;
  size_t currentOffset_;
};

TestParser::TestParser(const std::string& filename, std::string& contents)
    : filename_{filename}, contents_{contents} {
}

bool TestParser::parse(std::string* programText, std::vector<TestMessage>* messages) {
  *programText = parseUntilInitializer();

  while (!eof())
    messages->push_back(parseMessage());

  return true;
}

std::string TestParser::parseUntilInitializer() {
  for (;;) {
    size_t lastLineOffset = currentOffset_;
    std::string line = parseLine();
    if (line.empty() || line == "# ----") {
      return contents_.substr(0, lastLineOffset);
    }
  }
}

std::string TestParser::parseLine() {
  constexpr char LF = '\n';
  const size_t startOfLine = currentOffset_;

  while (!eof() && contents_[currentOffset_] != LF)
    currentOffset_++;

  std::string line = contents_.substr(startOfLine, currentOffset_ - startOfLine);

  if (!eof() && contents_[currentOffset_] == LF)
    currentOffset_++;

  return line;
}

TestMessage TestParser::parseMessage() {
  // TestMessage   ::= '#' AnalysisType ':' Location MessageText LF
  // MessageText   ::= TEXT (LF INDENT TEXT)*
  // AnalysisType  ::= 'TokenError' | 'SyntaxError' | 'TypeError' | 'Warning' | 'LinkError'
  // Location      ::= '[' FilePos ['..' FilePos] ']'
  // FilePos       ::= Line ':' Column
  // Column        ::= NUMBER
  // Line          ::= NUMBER

  parseCommentToken();
  AnalysisType type = parseAnalysisType();
  parseColon();
  SourceLocation location = parseLocation();
  std::string text = parseMessageText();
  parseLF();

  return TestMessage{};
}

void TestParser::parseCommentToken() {
  if (currentChar() == '#')
    nextChar();

  skipWhiteSpaces();
}

AnalysisType TestParser::parseAnalysisType() {
  const size_t startColumn = currentOffset_;
  const std::string stringValue = parseIdent();

  if (stringValue == "TokenError")
    return AnalysisType::TokenError;

  if (stringValue == "SyntaxError")
    return AnalysisType::SyntaxError;

  if (stringValue == "TypeError")
    return AnalysisType::TypeError;

  if (stringValue == "Warning")
    return AnalysisType::Warning;

  if (stringValue == "LinkError")
    return AnalysisType::LinkError;

  const size_t endColumn = currentOffset_;

  // XXX prints error message, dump currentLine() and underlines column from given start to end
  reportErrorAt(currentLine(), startColumn, endColumn, "Unknown analysis-type");
}

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
  int errorCount = 0;
  for (auto& dir: fs::recursive_directory_iterator(p))
    if (dir.path().extension() == ".flow")
      if (!testFile(dir.path().string()))
        errorCount++;

  return errorCount == 0;
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
