#pragma once

#include <xzero/defines.h>

#include <string>
#include <memory>
#include <list>

namespace xzero {
namespace testing {

#define EXPECT_NEAR(actual, expected, diff)
#define EXPECT_EQ(actual, expected)
#define EXPECT_NE(actual, expected)
#define EXPECT_TRUE(actual)
#define EXPECT_FALSE(actual)
#define EXPECT_EXCEPTION(exception, program)
#define EXPECT_THROW_STATUS(status, program)
#define TEST(testCase, testName) _CREATE_TEST(testCase, testName, ::xzero::testing::Test)
#define TEST_F(testFixture, testName)

// ############################################################################

int run(int argc, char** argv);

// ############################################################################

/**
 * interface to a single test.
 */
class Test {
 public:
  virtual ~Test() {}

  virtual void SetUp();
  virtual void TestBody();
  virtual void TearDown();
};

/**
 * API to create one kind of a test.
 */
class TestFactory {
  XZERO_DISABLE_COPY(TestFactory)
 public:
  TestFactory() {}
  virtual ~TestFactory() {}
  virtual std::unique_ptr<Test> createTest() = 0;
};

template<typename TheTestClass>
class TestFactoryTemplate : public TestFactory {
 public:
  std::unique_ptr<Test> createTest() override {
    return std::unique_ptr<Test>(new TheTestClass());
  }
};
#define _TEST_FACTORY(TheTestClass) TestFactoryTemplate<TheTestClass>

/**
 * Stores the result of a test run.
 */
class TestResult {
 public:
  TestResult() {};

  bool isSuccess() const noexcept { return success_; }
  bool isFailed() const noexcept { return !success_; }

 private:
  bool success_;
};

/**
 * TestInfo describes a single test.
 */
class TestInfo {
  XZERO_DISABLE_COPY(TestInfo)
 public:
  TestInfo(const std::string& testCaseName, 
           const std::string& testName,
           bool enabled,
           std::unique_ptr<TestFactory>&& testFactory);

 private:
  std::string testCaseName_;
  std::string testName_;
  bool enabled_;
  std::unique_ptr<TestFactory> testFactory_;
  TestResult testResult_;
};

class TestCase {
 public:
  void run();

 private:
  std::string name_;
  bool enabled_;
  std::list<TestInfo*> tests_;
};

class UnitTest {
 public:
  UnitTest();
  ~UnitTest();

  int run();

 private:
  std::list<std::unique_ptr<TestCase>> testCases_;
};

#define _TEST_CLASS_NAME(testCaseName, testName) \
  Test_##testCaseName##testName

#define _CREATE_TEST(testCaseName, testName, ParentClass)                     \
class _TEST_CLASS_NAME(testCaseName, testName) : public ParentClass {         \
 public:                                                                      \
  _TEST_CLASS_NAME(testCaseName, testName)() {}                               \
                                                                              \
 private:                                                                     \
  virtual void TestBody();                                                    \
                                                                              \
  static ::xzero::testing::TestInfo* const test_info_ XZERO_UNUSED;           \
};                                                                            \
                                                                              \
::xzero::testing::TestInfo* const                                             \
_TEST_CLASS_NAME(testCaseName, testName)::test_info_ =                        \
    ::xzero::testing::_registerTestInfo(                                      \
        #testCaseName, #testName,                                             \
        std::unique_ptr<::xzero::testing::TestFactory>(                       \
            new ::xzero::testing::TestFactoryTemplate<                        \
                _TEST_CLASS_NAME(testCaseName, testName)>));                   \
                                                                              \
void _TEST_CLASS_NAME(testCaseName, testName)::TestBody()

TestInfo* _registerTestInfo(const char* testCaseName,
                            const char* testName,
                            std::unique_ptr<TestFactory>&& testFactory);

} // namespace testing
} // namespace xzero

