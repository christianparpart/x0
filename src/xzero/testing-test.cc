#include <xzero/testing.h>
#include <xzero/logging.h>
#include <xzero/logging/ConsoleLogTarget.h>
#include <stdio.h>

TEST(Stub, foo) {
  xzero::Logger::get()->addTarget(xzero::ConsoleLogTarget::get());
  xzero::Logger::get()->setMinimumLogLevel(xzero::LogLevel::Info);

  xzero::logInfo("testing", "hello Stub.foo");
  printf("hello Stub.foo\n");
}

TEST(Stub, bar) {
  xzero::logInfo("testing", "hello Stub.bar");
  ASSERT_EQ(2, 3);
  xzero::logError("testing", "after assert fail");
  EXPECT_EQ(3, 4);
}

TEST(DISABLED_Fnord, foo) {
  xzero::logInfo("testing", "hello Stub.bar");
  EXPECT_EQ(2, 2);
}
