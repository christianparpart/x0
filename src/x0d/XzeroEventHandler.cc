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
}

XzeroEventHandler::~XzeroEventHandler() {
}

void XzeroEventHandler::onReload() {
  logNotice("x0d", "Reload signal received.");

  scheduler_->executeOnSignal(SIGHUP, std::bind(&XzeroEventHandler::onReload, this));

  daemon_->onCycleLogs();
}

} // namespace x0d
