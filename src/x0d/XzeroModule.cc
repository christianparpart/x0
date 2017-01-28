// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroModule.h>
#include <x0d/XzeroDaemon.h>
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
  SafeCall safeCall(std::unique_ptr<ExceptionHandler>(
        new CatchAndLogExceptionHandler("XzeroModule")));

  for (const auto& cleanup: cleanups_) {
    safeCall(cleanup);
  }
}

// {{{ hook setup API
void XzeroModule::onPostConfig() {
}

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
