// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/FlowParser.h>
#include <xzero-flow/ASTPrinter.h>
#include <xzero/logging/Logger.h>
#include <xzero/logging/LogLevel.h>
#include <xzero/testing.h>
#include <memory>

using namespace xzero;
using namespace xzero::flow;

TEST(FlowParser, handlerDecl) {
  auto parser = std::make_shared<FlowParser>(nullptr, nullptr, nullptr);
  parser->openString("handler main {}");
  std::unique_ptr<Unit> unit = parser->parse();

  auto h = unit->findHandler("main");
  ASSERT_TRUE(h != nullptr);

  CompoundStmt* body = dynamic_cast<CompoundStmt*>(h->body());
  ASSERT_TRUE(body != nullptr);
  EXPECT_TRUE(body->empty());
}

TEST(FlowParser, varDecl) {
  auto parser = std::make_shared<FlowParser>(nullptr, nullptr, nullptr);
  parser->openString("handler main { var i = 42; }");
  std::unique_ptr<Unit> unit = parser->parse();

  auto h = unit->findHandler("main");
  ASSERT_TRUE(h != nullptr);

  ASSERT_NE(nullptr, h->scope()->lookup("i", Lookup::Self));
  Symbol* var = h->scope()->lookup("i", Lookup::Self);

  ASSERT_TRUE(var != nullptr);
  EXPECT_EQ("i", var->name());
}

// TEST(FlowParser, logicExpr) {} // TODO
// TEST(FlowParser, notExpr) {} // TODO
// TEST(FlowParser, relExpr) {} // TODO
// TEST(FlowParser, addExpr) {} // TODO
// TEST(FlowParser, bitNotExpr) {} // TODO
// TEST(FlowParser, primaryExpr) {} // TODO
// TEST(FlowParser, arrayExpr) {} // TODO
// TEST(FlowParser, literalExpr) {} // TODO
// TEST(FlowParser, interpolatedStringExpr) {} // TODO
// TEST(FlowParser, castExpr) {} // TODO
// TEST(FlowParser, ifStmt) {} // TODO
// TEST(FlowParser, matchStmt) {} // TODO

TEST(FlowParser, forStmt_range2) {
  FlowParser parser;
  parser.openString("handler main { for var i, k in ['hello', 'world'] {} }");
  std::unique_ptr<Unit> unit = parser.parse();

  auto main = unit->findHandler("main");
  ASSERT_TRUE(main != nullptr);

  if (Logger::get()->getMinimumLogLevel() >= LogLevel::Debug) {
    ASTPrinter::print(main);
  }
}

// TEST(FlowParser, compoundStmt) {} // TODO
// TEST(FlowParser, callStmt) {} // TODO
// TEST(FlowParser, postscriptStmt) {} // TODO
