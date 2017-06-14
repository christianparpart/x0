// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero/StringUtil.h>

#include <string>
#include <memory>
#include <vector>

namespace xzero {
namespace testing {

#define TEST_ENV_SETUP(Name)                                                  \
  class _CALLBACK_NAME(Name) : public ::xzero::testing::Callback {            \
   public:                                                                    \
    void invoke() override;                                                   \
   private:                                                                   \
    static ::xzero::testing::Callback* const ref_ XZERO_UNUSED;               \
  };                                                                          \
                                                                              \
  ::xzero::testing::Callback* const                                           \
  _CALLBACK_NAME(Name)::ref_ =                                                \
      ::xzero::testing::UnitTest::instance()->addInitializer(                 \
          std::unique_ptr<::xzero::testing::Callback>(                        \
                new _CALLBACK_NAME(Name)));                                   \
                                                                              \
  void _CALLBACK_NAME(Name)::invoke()

#define _CALLBACK_NAME(Name) Callback_##Name

#define TEST_ENV_TEARDOWN(Name) // TODO

#define TEST_ENV_F(EnvName)                                                   \
  ::xzero::testing::UnitTest::instance()->addEnvironment(                     \
      std::unique_ptr<::xzero::testing::Environment>(EnvName));

// ############################################################################

#define TEST(testCase, testName) _CREATE_TEST(testCase, testName, ::xzero::testing::Test)
#define TEST_F(testFixture, testName) _CREATE_TEST(testFixture, testName, testFixture)

#define EXPECT_EQ(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, false, expected, actual, ==)

#define EXPECT_NE(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, false, expected, actual, !=)

#define EXPECT_GE(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, false, expected, actual, >=)

#define EXPECT_LE(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, false, expected, actual, <=)

#define EXPECT_GT(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, false, expected, actual, >)

#define EXPECT_LT(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, false, expected, actual, <)

#define EXPECT_TRUE(actual) \
  _EXPECT_BOOLEAN(__FILE__, __LINE__, false, true, actual)

#define EXPECT_FALSE(actual) \
  _EXPECT_BOOLEAN(__FILE__, __LINE__, false, false, actual)

#define EXPECT_NEAR(expected, actual, diff)       // TODO

#define EXPECT_THROW_STATUS(status, program)                                  \
  do {                                                                        \
    try {                                                                     \
      program;                                                                \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, false, #program, #status,                       \
          "<no exception thrown>");                                           \
    } catch (const RuntimeError& rt) {                                        \
      if (rt != ::xzero::Status:: status) {                                   \
        ::xzero::testing::UnitTest::instance()->reportEH(                     \
            __FILE__, __LINE__, false, #program, #status, rt.what());         \
      }                                                                       \
      break;                                                                  \
    } catch (...) {                                                           \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, false, #program, #status, "<foreign>");         \
    }                                                                         \
  } while (0)

#define EXPECT_THROW(program, ExceptionType)                                  \
  do {                                                                        \
    try {                                                                     \
      program;                                                                \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, false, #program, #ExceptionType,                \
          "<no exception thrown>");                                           \
    } catch (const ExceptionType&) {                                          \
      break; \
    } catch (...) { \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, false, #program, #ExceptionType, "<foreign>");  \
    }                                                                         \
  } while (0)

#define EXPECT_ANY_THROW(program)                                             \
  do {                                                                        \
    try {                                                                     \
      program;                                                                \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, false, #program, "<any exception>",             \
          "<no exception thrown>");                                           \
    } catch (...) {                                                           \
    }                                                                         \
  } while (0)

// ############################################################################

#define ASSERT_EQ(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, true, expected, actual, ==)

#define ASSERT_NE(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, true, expected, actual, !=)

#define ASSERT_GE(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, true, expected, actual, >=)

#define ASSERT_LE(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, true, expected, actual, <=)

#define ASSERT_GT(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, true, expected, actual, >)

#define ASSERT_LT(expected, actual) \
  _EXPECT_BINARY(__FILE__, __LINE__, true, expected, actual, <)

#define ASSERT_TRUE(actual) \
  _EXPECT_BOOLEAN(__FILE__, __LINE__, true, true, actual)

#define ASSERT_FALSE(actual) \
  _EXPECT_BOOLEAN(__FILE__, __LINE__, true, false, actual)

#define ASSERT_NEAR(expected, actual, diff)       // TODO

#define ASSERT_THROW_STATUS(status, program)                                  \
  do {                                                                        \
    try {                                                                     \
      program;                                                                \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, true, #program, #status,                        \
          "<no exception thrown>");                                           \
    } catch (const RuntimeError& rt) {                                        \
      if (rt != ::xzero::Status:: status) {                                   \
        ::xzero::testing::UnitTest::instance()->reportEH(                     \
            __FILE__, __LINE__, true, #program, #status, rt.what());          \
      }                                                                       \
      break;                                                                  \
    } catch (...) {                                                           \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, true, #program, #status, "<foreign>");          \
    }                                                                         \
  } while (0)

#define ASSERT_THROW(program, ExceptionType)                                  \
  do {                                                                        \
    try {                                                                     \
      program;                                                                \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, true, #program, #ExceptionType,                 \
          "<no exception thrown>");                                           \
    } catch (const ExceptionType&) {                                          \
      break; \
    } catch (...) { \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, true, #program, #ExceptionType, "<foreign>");   \
    }                                                                         \
  } while (0)

#define ASSERT_ANY_THROW(program)                                             \
  do {                                                                        \
    try {                                                                     \
      program;                                                                \
      ::xzero::testing::UnitTest::instance()->reportEH(                       \
          __FILE__, __LINE__, true, #program, "<any exception>",              \
          "<no exception thrown>");                                           \
    } catch (...) {                                                           \
    }                                                                         \
  } while (0)

// ############################################################################

#define _EXPECT_BOOLEAN(fileName, lineNo, fatal, expected, actual)            \
  do {                                                                        \
    bool actualEvaluated = !! (actual);                                       \
    bool failed = (expected && !actualEvaluated)                              \
               || (!expected && actualEvaluated);                             \
    if (failed) {                                                             \
      ::xzero::testing::UnitTest::instance()->reportBinary(                   \
          __FILE__, __LINE__, fatal, #expected, #actual,                      \
          ::xzero::StringUtil::toString(actualEvaluated), "");                \
    } \
  } while (0)

#define _EXPECT_BINARY(fileName, lineNo, fatal, expected, actual, op)         \
  do if (!(expected op actual)) {                                             \
    ::xzero::testing::UnitTest::instance()->reportBinary(                     \
        __FILE__, __LINE__, fatal, #expected, #actual,                        \
        ::xzero::StringUtil::toString(actual), #op);                          \
  } while (0)

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

// ############################################################################

int main(int argc, const char* argv[]);

// ############################################################################

class Callback {
 public:
  virtual ~Callback() {}

  virtual void invoke() = 0;
};

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

  void log(const std::string& message);

  template<typename... Args>
  void logf(const char* fmt, Args... args);
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

  Callback* addInitializer(std::unique_ptr<Callback>&& cb);

  TestInfo* addTest(const char* testCaseName,
                    const char* testName,
                    std::unique_ptr<TestFactory>&& testFactory);

  void reportBinary(const char* fileName,
                    int lineNo,
                    bool fatal,
                    const char* expected,
                    const char* actual,
                    const std::string& actualEvaluated,
                    const char* op);

  void reportEH(const char* fileName,
                int lineNo,
                bool fatal,
                const char* program,
                const char* expected,
                const char* actual);

  void reportMessage(const std::string& message, bool fatal);

  void log(const std::string& message);

  template<typename ... Args>
  void logf(const char* format, Args... args) {
    log(StringUtil::format(format, args...));
  }

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
  std::vector<std::unique_ptr<Callback>> initializers_;
  std::vector<std::unique_ptr<TestInfo>> testCases_;

  //! ordered list of tests as offsets into testCases_
  std::vector<size_t> activeTests_;

  std::string filter_;
  int repeats_;
  bool printProgress_;
  bool printSummaryDetails_;

  TestInfo* currentTestCase_;
  size_t currentCount_;
  size_t successCount_;
  int failCount_;
  std::vector<std::string> failures_;
};

template<typename... Args>
inline void Test::logf(const char* fmt, Args... args) {
  UnitTest::instance()->logf(fmt, args...);
}

} // namespace testing
} // namespace xzero
