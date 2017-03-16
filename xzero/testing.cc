// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/testing.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/logging/Logger.h>
#include <xzero/Application.h>
#include <xzero/AnsiColor.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdlib>
#include <fnmatch.h>

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

Environment::~Environment() {
}

void Environment::SetUp() {
}

void Environment::TearDown() {
}

// ############################################################################

Test::~Test() {
}

void Test::SetUp() {
}

void Test::TearDown() {
}

void Test::log(const std::string& message) {
  UnitTest::instance()->log(message);
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
static std::string colorsError = AnsiColor::make(AnsiColor::Red | AnsiColor::Bold);
static std::string colorsOk = AnsiColor::make(AnsiColor::Green);

UnitTest::UnitTest()
  : environments_(),
    testCases_(),
    activeTests_(),
    filter_("*"),
    repeats_(1),
    printProgress_(false),
    printSummaryDetails_(true),
    currentTestCase_(nullptr),
    currentCount_(0),
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

  std::shuffle(activeTests_.begin(), activeTests_.end(),
      std::default_random_engine(seed));
}

void UnitTest::sortTestsAlphabetically() {
  std::sort(
      activeTests_.begin(),
      activeTests_.end(),
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
     .defineBool("verbose", 'v', "Prints to console in debug log level.")
     .defineString("log-level", 'L', "ENUM", "Defines the minimum log level.", "", nullptr)
     .defineString("log-target", 0, "ENUM", "Specifies logging target. One of syslog, file, systemd, console.", "", nullptr)
     .defineString("filter", 'f', "GLOB", "Filters tests by given glob.", "*")
     .defineBool("list", 'l', "Prints all tests and exits.")
     .defineBool("randomize", 'R', "Randomizes test order.")
     .defineBool("sort", 's', "Sorts tests alphabetically ascending.")
     .defineBool("no-progress", 0, "Avoids printing progress.")
     .defineNumber("repeat", 'r', "COUNT", "Repeat tests given number of times.", 1)
     ;

  Flags flags = cli.evaluate(argc, argv);

  if (flags.getBool("help")) {
    printf("%s\n", cli.helpText().c_str());
    return 0;
  }

  if (flags.getBool("verbose")) {
    Application::logToStderr(LogLevel::Debug);
  }

  if (flags.isSet("log-level")) {
    Logger::get()->setMinimumLogLevel(make_loglevel(flags.getString("log-level")));
  }

  if (flags.isSet("log-target")) {
    std::string logTargetStr = flags.getString("log-target");
    // TODO: log-target
  }

  filter_ = flags.getString("filter");
  repeats_ = flags.getNumber("repeat");
  printProgress_ = !flags.getBool("no-progress");

  if (flags.getBool("randomize"))
    randomizeTestOrder();
  else if (flags.getBool("sort"))
    sortTestsAlphabetically();

  { // if (filter_ != "*") {
    std::vector<size_t> filtered;
    for (size_t i = 0, e = activeTests_.size(); i != e; ++i) {
      TestInfo* testInfo = testCases_[activeTests_[i]].get();
      std::string matchName = StringUtil::format("$0.$1",
          testInfo->testCaseName(), testInfo->testName());
      const int flags = 0;

      if (fnmatch(filter_.c_str(), matchName.c_str(), flags) == 0) {
        filtered.push_back(activeTests_[i]);
      }
    }
    activeTests_ = std::move(filtered);
  }

  if (flags.getBool("list")) {
    printTestList();
    return 0;
  }

  for (auto& env: environments_) {
    env->SetUp();
  }

  for (auto& init: initializers_) {
    init->invoke();
  }

  for (int i = 0; i < repeats_; i++) {
    runAllTestsOnce();
  }

  for (auto& env: environments_) {
    env->TearDown();
  }

  printSummary();

  return failCount_ == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void UnitTest::printTestList() {
  for (size_t i = 0, e = activeTests_.size(); i != e; ++i) {
    TestInfo* testCase = testCases_[activeTests_[i]].get();
    printf("%4zu. %s.%s\n",
        i + 1,
        testCase->testCaseName().c_str(),
        testCase->testName().c_str());
  }
}

void UnitTest::printSummary() {
  // print summary
  printf("%sFinished running %d repeats, %zu tests. %zu success, %d failed, %zu disabled.%s\n",
      failCount_ ? colorsError.c_str()
                : colorsOk.c_str(),
      repeats_,
      activeTests_.size(),
      successCount_,
      failCount_,
      disabledCount(),
      colorsReset.c_str());

  if (printSummaryDetails_ && !failures_.empty()) {
    printf("================================\n");
    printf(" Summary:\n");
    printf("================================\n");

    for (size_t i = 0, e = failures_.size(); i != e; ++i) {
      const auto& failure = failures_[i];
      printf("%s%s%s\n",
          colorsError.c_str(),
          failure.c_str(),
          colorsReset.c_str());
    }
  }
}

size_t UnitTest::enabledCount() const {
  size_t count = 0;

  for (size_t i = 0, e = activeTests_.size(); i != e; ++i) {
    if (testCases_[activeTests_[i]]->isEnabled()) {
      count++;
    }
  }

  return count;
}

size_t UnitTest::disabledCount() const {
  size_t count = 0;

  for (size_t i = 0, e = activeTests_.size(); i != e; ++i) {
    if (!testCases_[activeTests_[i]]->isEnabled()) {
      count++;
    }
  }

  return count;
}

void UnitTest::runAllTestsOnce() {
  const int totalCount = repeats_ * enabledCount();

  for (size_t i = 0, e = activeTests_.size(); i != e; ++i) {
    TestInfo* testCase = testCases_[activeTests_[i]].get();
    std::unique_ptr<Test> test = testCase->createTest();

    if (!testCase->isEnabled())
      continue;

    currentTestCase_ = testCase;
    currentCount_++;
    int percentage = currentCount_ * 100 / totalCount;

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
      failed++;
    } catch (...) {
      // TODO: report failure upon set-up phase, hence skipping actual test
      failed++;
    }

    if (!failed) {
      try {
        test->TestBody();
      } catch (const BailOutException&) {
        // no-op
        failed++;
      } catch (const RuntimeError& ex) {
        reportMessage(
            StringUtil::format(
              "Unhandled exception caught in test. $0 ($1:$2 $3)",
              ex.what(),
              ex.sourceFile(),
              ex.sourceLine(),
              ex.functionName()),
            false);
        failed++;
      } catch (const std::exception& ex) {
        reportMessage(
            StringUtil::format("Unhandled exception caught in test. $0", ex.what()),
            false);
        failed++;
      } catch (...) {
        reportMessage("Unhandled exception caught in test.", false);
        failed++;
      }

      try {
        test->TearDown();
      } catch (const BailOutException&) {
        // SHOULD NOT HAPPEND: complain about it
        failed++;
      } catch (...) {
        // TODO: report failure in tear-down
        failed++;
      }

      if (!failed) {
        successCount_++;
      }
    }
  }
}

void UnitTest::reportBinary(const char* fileName,
                            int lineNo,
                            bool fatal,
                            const char* expected,
                            const char* actual,
                            const std::string& actualEvaluated,
                            const char* op) {
  std::string message = StringUtil::format(
      "$0:$1: Failure\n"
      "  Value of: $2\n"
      "  Expected: $3 $4\n"
      "    Actual: $5\n",
      fileName, lineNo,
      actual,
      expected, op,
      actualEvaluated);

  reportMessage(message, fatal);
}

void UnitTest::reportEH(const char* fileName,
                        int lineNo,
                        bool fatal,
                        const char* program,
                        const char* expected,
                        const char* actual) {
  std::string message = StringUtil::format(
      "$0:$1: $2\n"
      "  Value of: $3\n"
      "  Expected: $4\n"
      "    Actual: $5\n",
      fileName, lineNo,
      actual ? "Unexpected exception caught"
             : "No exception caught",
      program,
      expected,
      actual);

  reportMessage(message, fatal);
}

void UnitTest::reportMessage(const std::string& message, bool fatal) {
  printf("%s%s%s\n",
      colorsError.c_str(),
      message.c_str(),
      colorsReset.c_str());

  failCount_++;
  failures_.emplace_back(message);

  if (fatal) {
    throw BailOutException();
  }
}

void UnitTest::addEnvironment(std::unique_ptr<Environment>&& env) {
  environments_.emplace_back(std::move(env));
}

Callback* UnitTest::addInitializer(std::unique_ptr<Callback>&& cb) {
  initializers_.emplace_back(std::move(cb));
  return initializers_.back().get();
}

TestInfo* UnitTest::addTest(const char* testCaseName,
                            const char* testName,
                            std::unique_ptr<TestFactory>&& testFactory) {
  testCases_.emplace_back(
      new TestInfo(
          testCaseName,
          testName,
          !StringUtil::beginsWith(testCaseName, "DISABLED_")
              && !StringUtil::beginsWith(testName, "DISABLED_"),
          std::move(testFactory)));

  activeTests_.emplace_back(activeTests_.size());

  return testCases_.back().get();
}

void UnitTest::log(const std::string& message) {
  std::string component = StringUtil::format("$0.$1",
      currentTestCase_->testCaseName(),
      currentTestCase_->testName());
  logDebug(component, "$0", message);
}

} // namespace testing
} // namespace xzero
