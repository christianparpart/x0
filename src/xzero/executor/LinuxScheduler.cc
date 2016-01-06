// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
