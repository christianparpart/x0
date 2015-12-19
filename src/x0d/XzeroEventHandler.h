// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "XzeroState.h"

namespace xzero {
  class HttpServer;
  class Scheduler;
}

namespace x0d {

class XzeroDaemon;

class XzeroEventHandler {
 public:
  XzeroEventHandler(XzeroDaemon* daemon, xzero::Scheduler* scheduler);
  ~XzeroEventHandler();

  xzero::Scheduler* scheduler() const { return scheduler_; }

  xzero::HttpServer* server() const;

  XzeroState state() const { return state_; }
  void setState(XzeroState newState);

  void setupChild(int pid);

 private:
  void onReload();
  void onTerminate();

  // void reopenLogsHandler(ev::sig&, int);
  // void reexecHandler(ev::sig& sig, int);
  // void onChild(ev::child&, int);
  // void suspendHandler(ev::sig& sig, int);
  // void resumeHandler(ev::sig& sig, int);
  // void gracefulShutdownHandler(ev::sig& sig, int);
  // void quickShutdownHandler(ev::sig& sig, int);
  // void quickShutdownTimeout(ev::timer&, int);

  // void logLevelInc(ev::sig& sig, int);
  // void logLevelDec(ev::sig& sig, int);

 private:
  XzeroDaemon* daemon_;
  xzero::Scheduler* scheduler_;
  XzeroState state_;
  // ev::sig terminateSignal_;
  // ev::sig ctrlcSignal_;
  // ev::sig quitSignal_;
  // ev::sig user1Signal_;
  // ev::sig reloadSignal_;
  // ev::sig suspendSignal_;
  // ev::sig resumeSignal_;
  // ev::sig logLevelIncSignal_;
  // ev::sig logLevelDecSignal_;
  // ev::timer terminationTimeout_;
  // ev::child child_;
};

}  // namespace x0d

