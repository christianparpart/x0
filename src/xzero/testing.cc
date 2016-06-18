// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/Application.h>
#include <xzero/AnsiColor.h>
#include <xzero/Buffer.h>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdlib>

namespace xzero {
namespace testing {

int main(int argc, const char* argv[]) {
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

UnitTest::UnitTest()
  : testCases_(),
    testOrder_(),
    repeats_(1),
    randomize_(false),
    printProgress_(false),
    printSummaryDetails_(false),
    currentCount_(0),
    totalCount_(0),
    successCount_(0),
    failCount_(0) {
}

UnitTest::~UnitTest() {
}

UnitTest* UnitTest::instance() {
  static UnitTest unitTest;
  return &unitTest;
}

void UnitTest::randomizeTestOrder() {
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

  std::shuffle(testOrder_.begin(), testOrder_.end(),
      std::default_random_engine(seed));
}

void UnitTest::sortTestsAlphabetically() {
  std::sort(
      testOrder_.begin(),
      testOrder_.end(),
      [this](size_t a, size_t b) -> bool {
        TestInfo* left = testCases_[a].get();
        TestInfo* right = testCases_[b].get();

        if (left->testCaseName() < right->testCaseName())
          return true;

        if (left->testCaseName() == right->testCaseName())
          return left->testName() < right->testName();

        return false;
      });
}

int UnitTest::main(int argc, const char* argv[]) {
  // TODO: add CLI parameters (preferably gtest compatible)
  //
  // --no-color | --color   explicitely enable/disable color output
  // --filter=REGEX         filter tests by regular expression
  // --randomize            randomize test order
  // --repeats=NUMBER        repeats tests given number of times
  // --list[-tests]         Just list the tests and exit.

  Application::init();

  CLI cli;
  cli.defineBool("help", 'h', "Prints this help and terminates.")
     .defineString("log-level", 'L', "ENUM", "Defines the minimum log level.", "info", nullptr)
     .defineString("log-target", 0, "ENUM", "Specifies logging target. One of syslog, file, systemd, console.", "console", nullptr)
     .defineBool("list", 'l', "Prints all tests and exits.")
     .defineBool("randomize", 'R', "Randomizes test order.")
     .defineBool("no-progress", 0, "Avoids printing progress.")
     .defineNumber("repeat", 'r', "COUNT", "Repeat tests given number of times.", 1)
     ;

  Flags flags = cli.evaluate(argc, argv);

  if (flags.getBool("help")) {
    printf("%s\n", cli.helpText().c_str());
    return 0;
  }

  repeats_ = flags.getNumber("repeat");
  randomize_ = flags.getBool("randomize");
  printProgress_ = !flags.getBool("no-progress");

  totalCount_ = repeats_ * enabledCount();

  if (randomize_)
    randomizeTestOrder();
  else
    sortTestsAlphabetically();

  if (flags.getBool("list")) {
    printTestList();
    return 0;
  }

  for (int i = 0; i < repeats_; i++) {
    runAllTestsOnce();
  }

  printSummary();

  return failCount_ == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
void UnitTest::printTestList() {
  for (size_t i = 0, e = testOrder_.size(); i != e; ++i) {
    TestInfo* testCase = testCases_[testOrder_[i]].get();
    printf("%4zu. %s.%s\n",
        i + 1,
        testCase->testCaseName().c_str(),
        testCase->testName().c_str());
  }
}

void UnitTest::printSummary() {
  // print summary
  printf("%sFinished running %d repeats, %zu tests. %d success %d failed, %zu disabled.%s\n",
      failCount_ ? colorsError.c_str()
                : colorsOk.c_str(),
      repeats_,
      testCases_.size(),
      successCount_,
      failCount_,
      disabledCount(),
      colorsReset.c_str());

  if (printSummaryDetails_) {
    // TODO: print summary details
  }
}

size_t UnitTest::enabledCount() const {
  return testCases_.size() - disabledCount();
}

size_t UnitTest::disabledCount() const {
  size_t count = 0;

  for (const auto& info: testCases_) {
    if (!info->isEnabled())
      count++;
  }
  return count;
}

void UnitTest::runAllTestsOnce() {
  for (size_t i = 0, e = testOrder_.size(); i != e; ++i) {
    TestInfo* testCase = testCases_[testOrder_[i]].get();
    std::unique_ptr<Test> test = testCase->createTest();

    if (!testCase->isEnabled()) {
      if (printProgress_) {
        printf("%sSkipping test: %s.%s%s\n",
            colorsTestCaseHeader.c_str(),
            testCase->testCaseName().c_str(),
            testCase->testName().c_str(),
            colorsReset.c_str());
      }

      continue;
    }

    currentCount_++;
    int percentage = currentCount_ * 100 / totalCount_;

    if (printProgress_) {
      printf("%s%3d%% Running test: %s.%s%s\n",
          colorsTestCaseHeader.c_str(),
          percentage,
          testCase->testCaseName().c_str(),
          testCase->testName().c_str(),
          colorsReset.c_str());
    }

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
    } else {
      successCount_++;
    }
  }
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

  testOrder_.emplace_back(testOrder_.size());

  return testCases_.back().get();
}

} // namespace testing
} // namespace xzero
