// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "XzeroModule.h"
#include "XzeroDaemon.h"
#include <xzero/logging.h>
#include <xzero/executor/SafeCall.h>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

XzeroModule::XzeroModule(XzeroDaemon* x0d, const std::string& name)
    : daemon_(x0d),
      name_(name),
      cleanups_(),
      natives_() {
}

XzeroModule::~XzeroModule() {
  SafeCall safeCall(UniquePtr<ExceptionHandler>(
        new CatchAndLogExceptionHandler("XzeroModule")));

  for (const auto& cleanup: cleanups_) {
    safeCall(cleanup);
  }
}

// {{{ hook setup API
void XzeroModule::onCycleLogs(std::function<void()> cb) {
  auto handle = daemon().onCycleLogs.connect(cb);
  cleanups_.push_back([=]() { daemon().onCycleLogs.disconnect(handle); });
}

void XzeroModule::onConnectionOpen(std::function<void(xzero::Connection*)> cb) {
  auto handle = daemon().onConnectionOpen.connect(cb);
  cleanups_.push_back([=]() {
    daemon().onConnectionOpen.disconnect(handle);
  });
}

void XzeroModule::onConnectionClose(std::function<void(xzero::Connection*)> cb) {
  auto handle = daemon().onConnectionClose.connect(cb);
  cleanups_.push_back([=]() {
    daemon().onConnectionClose.disconnect(handle);
  });
}

void XzeroModule::onPreProcess(
    std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> cb) {
  auto handle = daemon().onPreProcess.connect(cb);
  cleanups_.push_back([=]() { daemon().onPreProcess.disconnect(handle); });
}

void XzeroModule::onPostProcess(
    std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> cb) {
  auto handle = daemon().onPostProcess.connect(cb);
  cleanups_.push_back([=]() { daemon().onPostProcess.disconnect(handle); });
}

void XzeroModule::onRequestDone(
    std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> cb) {
  auto handle = daemon().onRequestDone.connect(cb);
  cleanups_.push_back([=]() { daemon().onRequestDone.disconnect(handle); });
}
// }}}

} // namespace x0d
