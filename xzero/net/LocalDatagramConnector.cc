// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/LocalDatagramConnector.h>
#include <xzero/net/LocalDatagramEndPoint.h>
#include <xzero/executor/Executor.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

namespace xzero {

LocalDatagramConnector::LocalDatagramConnector(
    const std::string& name,
    DatagramHandler handler,
    Executor* executor)
    : DatagramConnector(name, handler, executor),
      isStarted_(false),
      pending_() {
}

LocalDatagramConnector::~LocalDatagramConnector() {
}

void LocalDatagramConnector::start() {
  if (isStarted_)
    RAISE(IllegalStateError);

  isStarted_ = true;

  runQueue();
}

bool LocalDatagramConnector::isStarted() const {
  return isStarted_;
}

void LocalDatagramConnector::stop() {
  if (!isStarted_)
    RAISE(IllegalStateError);

  isStarted_ = false;
}

RefPtr<LocalDatagramEndPoint> LocalDatagramConnector::send(
    const BufferRef& message) {
  Buffer buf;
  buf.push_back(message);

  return send(std::move(buf));
}

RefPtr<LocalDatagramEndPoint> LocalDatagramConnector::send(Buffer&& message) {
  RefPtr<LocalDatagramEndPoint> ep(
      new LocalDatagramEndPoint(this, std::move(message)));

  pending_.push_back(ep);

  runQueue();

  return ep;
}

void LocalDatagramConnector::runQueue() {
  auto rq = std::move(pending_);

  if (!handler_) {
    logDebug("LocalDatagramConnector",
             "runQueue: Ignore pending messages. No handler set.");
    return;
  }

  while (!rq.empty()) {
    auto ep = rq.front();
    rq.pop_front();
    executor_->execute(std::bind(handler_, ep.as<DatagramEndPoint>()));
  }
}

} // namespace xzero
