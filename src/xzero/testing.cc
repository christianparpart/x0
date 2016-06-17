#include <xzero/testing.h>

namespace xzero {
namespace testing {

int run(int argc, char** argv) {
  return 0;
}

// ############################################################################

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
    testFactory(std::move(testFactory)) {
}

// ############################################################################

void TestCase::run() {
  for (TestInfo* testInfo: tests_) {
    std::unique_ptr<Test> test = testInfo->createTest();

    test->SetUp();
    test->TestBody();
    test->TearDown();
  }
}

// ############################################################################

int UnitTest::run() {
  for (std::unique_ptr<TestCase>& testCase: testCases_) {
    testCase->run();
  }

  return 0;
}

// ############################################################################

TestInfo* _registerTestInfo(
    const char* testCaseName,
    const char* testName,
    const char* parentID,
    std::unique_ptr<TestFactory>&& testFactory) {
  return nullptr; // TODO
}

} // namespace testing
} // namespace xzero
