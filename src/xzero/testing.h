// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>

#include <string>
#include <memory>
#include <vector>

namespace xzero {
namespace testing {

#define TEST_ENV_SETUP(Name)    // TODO
#define TEST_ENV_TEARDOWN(Name) // TODO

#define TEST_ENV_F(EnvName)                                                   \
  ::xzero::testing::UnitTest::instance()->addEnvironment(                     \
      std::unique_ptr<::xzero::testing::Environment>(EnvName));

// ############################################################################

#define TEST(testCase, testName) _CREATE_TEST(testCase, testName, ::xzero::testing::Test)
#define TEST_F(testFixture, testName) _CREATE_TEST(testFixture, testName, testFixture)

#define FAIL(actual, expected)                                                \
    ::xzero::testing::UnitTest::instance()->reportFailure(                    \
        __FILE__, __LINE__, #expected, #actual, false);

#define EXPECT_EQ(actual, expected)                                           \
  do if ((actual) != (expected)) {                                            \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_NE(actual, expected)                                           \
  do if ((actual) != (expected)) {                                            \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_GE(actual, expected)                                           \
  do if (!((actual) >= (expected))) {                                         \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_LE(actual, expected)                                           \
  do if (!((actual) <= (expected))) {                                         \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_GT(actual, expected)                                           \
  do if (!((actual) > (expected))) {                                         \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_LT(actual, expected)                                           \
  do if (!((actual) < (expected))) {                                         \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_TRUE(actual)                                                   \
  do if (!static_cast<bool>(actual)) {                                        \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_FALSE(actual)                                                  \
  do if (static_cast<bool>(actual)) {                                         \
    FAIL((actual), (expected));                                               \
  } while (0)

#define EXPECT_NEAR(actual, expected, diff)       // TODO
#define EXPECT_EXCEPTION(ExceptionType, program)  // TODO
#define EXPECT_THROW(ExceptionType, program)      // TODO
#define EXPECT_ANY_THROW(program)                 // TODO
#define EXPECT_THROW_STATUS(status, program)      // TODO

// ############################################################################

#define FAIL_HARD(actual, expected)                                           \
    ::xzero::testing::UnitTest::instance()->reportFailure(                    \
        __FILE__, __LINE__, #expected, #actual, true);

#define ASSERT_EQ(actual, expected)                                           \
  do if ((actual) != (expected)) {                                            \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)                                                               

#define ASSERT_NE(actual, expected)                                           \
  do if ((actual) != (expected)) {                                            \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_GE(actual, expected)                                           \
  do if (!((actual) >= (expected))) {                                         \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_LE(actual, expected)                                           \
  do if (!((actual) <= (expected))) {                                         \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_GT(actual, expected)                                           \
  do if (!((actual) > (expected))) {                                          \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_LT(actual, expected)                                           \
  do if (!((actual) < (expected))) {                                          \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_TRUE(actual)                                                   \
  do if (!static_cast<bool>(actual)) {                                        \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_FALSE(actual)                                                  \
  do if (static_cast<bool>(actual)) {                                         \
    FAIL_HARD((actual), (expected));                                          \
  } while (0)

#define ASSERT_NEAR(actual, expected, diff)       // TODO
#define ASSERT_EXCEPTION(ExceptionType, program)  // TODO
#define ASSERT_ANY_THROW(program)                 // TODO
#define ASSERT_THROW(ExceptionType, program)      // TODO
#define ASSERT_THROW_STATUS(status, program)      // TODO

// ############################################################################

int main(int argc, const char* argv[]);

// ############################################################################

/**
 * Environment hooks.
 */
class Environment {
 public:
  virtual ~Environment();

  virtual void SetUp();
  virtual void TearDown();
};

/**
 * interface to a single test.
 */
class Test {
 public:
  virtual ~Test();

  virtual void SetUp();
  virtual void TestBody() = 0;
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

  const std::string& testCaseName() const { return testCaseName_; }
  const std::string& testName() const { return testName_; }
  bool isEnabled() const { return enabled_; }

  std::unique_ptr<Test> createTest() { return testFactory_->createTest(); }

 private:
  std::string testCaseName_;
  std::string testName_;
  bool enabled_;
  std::unique_ptr<TestFactory> testFactory_;
};

class UnitTest {
 public:
  UnitTest();
  ~UnitTest();

  static UnitTest* instance();

  int main(int argc, const char* argv[]);

  void addEnvironment(std::unique_ptr<Environment>&& env);

  TestInfo* addTest(const char* testCaseName,
                    const char* testName,
                    std::unique_ptr<TestFactory>&& testFactory);

  void reportFailure(const char* fileName,
                     int lineNo,
                     const char* actual,
                     const char* expected,
                     bool bailOut);

 private:
  void randomizeTestOrder();
  void sortTestsAlphabetically();
  void printTestList();
  void runAllTestsOnce();
  void printSummary();
  size_t enabledCount() const;
  size_t disabledCount() const;

 private:
  std::vector<std::unique_ptr<Environment>> environments_;
  std::vector<std::unique_ptr<TestInfo>> testCases_;

  //! ordered list of tests as offsets into testCases_
  std::vector<size_t> testOrder_;

  std::string filter_;
  int repeats_;
  bool randomize_;
  bool printProgress_;
  bool printSummaryDetails_;

  size_t currentCount_;
  int successCount_;
  int failCount_;
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
    ::xzero::testing::UnitTest::instance()->addTest(                          \
        #testCaseName, #testName,                                             \
        std::unique_ptr<::xzero::testing::TestFactory>(                       \
            new ::xzero::testing::TestFactoryTemplate<                        \
                _TEST_CLASS_NAME(testCaseName, testName)>));                  \
                                                                              \
void _TEST_CLASS_NAME(testCaseName, testName)::TestBody()

} // namespace testing
} // namespace xzero
