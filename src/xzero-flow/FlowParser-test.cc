// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/FlowParser.h>
#include <gtest/gtest.h>
#include <memory>

using namespace xzero;
using namespace xzero::flow;

TEST(FlowParser, handlerDecl) {
  auto parser = std::make_shared<FlowParser>(nullptr, nullptr, nullptr);
  parser->openString("handler main {}");
  std::unique_ptr<Unit> unit = parser->parse();

  auto h = unit->findHandler("main");
  ASSERT_TRUE(h != nullptr);
}

// TEST(FlowParser, varDecl) {} // TODO
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
// TEST(FlowParser, compoundStmt) {} // TODO
// TEST(FlowParser, callStmt) {} // TODO
// TEST(FlowParser, postscriptStmt) {} // TODO
