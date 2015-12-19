#include "XzeroEventHandler.h"
#include "XzeroDaemon.h"
#include <xzero/executor/Scheduler.h>
#include <xzero/logging.h>

namespace x0d {

using namespace xzero;

XzeroEventHandler::XzeroEventHandler(XzeroDaemon* daemon,
                                     xzero::Scheduler* scheduler)
    : daemon_(daemon),
      scheduler_(scheduler),
      state_(XzeroState::Inactive) {

  scheduler_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onReload, this));
  scheduler_->executeOnSignal(SIGTERM, std::bind(&XzeroEventHandler::onTerminate, this));
}

XzeroEventHandler::~XzeroEventHandler() {
}

void XzeroEventHandler::onReload() {
  logNotice("x0d", "Reload signal received.");

  daemon_->onCycleLogs();

  scheduler_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onReload, this));
}

void XzeroEventHandler::onTerminate() {
  logNotice("x0d", "Initiate termination.");
  daemon_->terminate();
}

} // namespace x0d
