#include <xzero/executor/LinuxScheduler.h>
#include <xzero/WallClock.h>

namespace xzero {

LinuxScheduler::LinuxScheduler(
    std::function<void(const std::exception&)> errorLogger,
    WallClock* clock,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : Scheduler(errorLogger), 
      clock_(clock ? clock : WallClock::monotonic()),
      onPreInvokePending_(preInvoke),
      onPostInvokePending_(postInvoke),
      readerCount_(0),
      writerCount_(0)
{
}

LinuxScheduler::LinuxScheduler(
    std::function<void(const std::exception&)> errorLogger,
    WallClock* clock)
    : LinuxScheduler(errorLogger, clock, nullptr, nullptr) {
}

LinuxScheduler::LinuxScheduler()
    : LinuxScheduler(nullptr, nullptr) {
}

LinuxScheduler::~LinuxScheduler() {
}


} // namespace xzero
