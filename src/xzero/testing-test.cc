#include <xzero/testing.h>
#include <xzero/logging.h>
#include <stdio.h>

TEST(Stub, foo) {
  xzero::Logger::get()->addTarget(xzero::ConsoleLogTarget::get());
  xzero::Logger::get()->setMinimumLogLevel(xzero::LogLevel::Info);

  xzero::logInfo("hello Stub.foo");
  printf("hello Stub.foo\n");
}

TEST(Stub, bar) {
  xzero::logInfo("hello Stub.bar");
  ASSERT_EQ(2, 3);
  xzero::logError("after assert fail");
  EXPECT_EQ(3, 4);
}

TEST(DISABLED_Fnord, foo) {
  xzero::logInfo("hello Stub.bar");
  EXPECT_EQ(2, 2);
}
