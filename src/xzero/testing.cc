// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/AnsiColor.h>
#include <xzero/Buffer.h>
#include <cstdlib>

namespace xzero {
namespace testing {

int main(int argc, char** argv) {
  return UnitTest::instance()->main(argc, argv);
}

// ############################################################################

class BailOutException {
 public:
  BailOutException() {}
};

// ############################################################################

Test::~Test() {
}

void Test::SetUp() {
}

void Test::TestBody() {
}

void Test::TearDown() {
}

// ############################################################################

TestInfo::TestInfo(const std::string& testCaseName, 
    const std::string& testName,
    bool enabled,
    std::unique_ptr<TestFactory>&& testFactory)
  : testCaseName_(testCaseName),
    testName_(testName),
    enabled_(enabled),
    testFactory_(std::move(testFactory)) {
}

// ############################################################################

static std::string colorsReset = AnsiColor::make(AnsiColor::Reset);
static std::string colorsTestCaseHeader = AnsiColor::make(AnsiColor::Cyan);
static std::string colorsError = AnsiColor::make(AnsiColor::Red);
static std::string colorsOk = AnsiColor::make(AnsiColor::Green);

UnitTest::UnitTest() {
}

UnitTest::~UnitTest() {
}

UnitTest* UnitTest::instance() {
  static UnitTest unitTest;
  return &unitTest;
}

int UnitTest::main(int /*argc*/, char** /*argv*/) {
  // TODO: add CLI parameters (preferably gtest compatible)
  //
  // --no-color | --color   explicitely enable/disable color output
  // --filter=REGEX         filter tests by regular expression
  // --randomize            randomize test order
  // --repeat=NUMBER        repeat tests given number of times

  failCount_ = 0;
  int disabledCount = 0;

  for (auto& testCase: testCases_) {
    std::unique_ptr<Test> test = testCase->createTest();

    if (!testCase->isEnabled()) {
      disabledCount++;

      printf("%sSkipping test: %s.%s%s\n",
          colorsTestCaseHeader.c_str(),
          testCase->testCaseName().c_str(),
          testCase->testName().c_str(),
          colorsReset.c_str());

      continue;
    }

    printf("%sRunning test: %s.%s%s\n",
        colorsTestCaseHeader.c_str(),
        testCase->testCaseName().c_str(),
        testCase->testName().c_str(),
        colorsReset.c_str());

    int failed = 0;

    try {
      test->SetUp();
    } catch (const BailOutException&) {
      // SHOULD NOT HAPPEND: complain about it
    } catch (...) {
      failed++;
    }

    if (!failed) {
      try {
        test->TestBody();
      } catch (const BailOutException&) {
        // no-op
      } catch (...) {
        failed++;
      }
    }

    try {
      test->TearDown();
    } catch (const BailOutException&) {
      // SHOULD NOT HAPPEND: complain about it
    } catch (...) {
      failed++;
    }

    if (failed != 0) {
      failCount_++;
    }
  }

  printf("%sFinished running %zu tests. %d failed, %d disabled.%s\n",
      failCount_ ? colorsError.c_str()
                : colorsOk.c_str(),
      testCases_.size(),
      failCount_,
      disabledCount,
      colorsReset.c_str());

  return failCount_ == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void UnitTest::reportFailure(const char* fileName,
                             int lineNo,
                             const char* actual,
                             const char* expected,
                             bool bailOut) {
  printf("%sExpected %s but got %s in %s:%d%s\n",
      colorsError.c_str(),
      expected,
      actual,
      fileName,
      lineNo,
      colorsReset.c_str());

  failCount_++;

  if (bailOut) {
    throw BailOutException();
  }
}

TestInfo* UnitTest::addTest(const char* testCaseName,
                            const char* testName,
                            std::unique_ptr<TestFactory>&& testFactory) {
  testCases_.emplace_back(
      new TestInfo(
          testCaseName,
          testName,
          !BufferRef(testCaseName).begins("DISABLED_"),
          std::move(testFactory)));

  return testCases_.back().get();
}

} // namespace testing
} // namespace xzero
