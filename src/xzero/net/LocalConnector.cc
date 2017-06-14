// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/LocalConnector.h>
#include <xzero/net/Connection.h>
#include <xzero/logging.h>
#include <xzero/executor/Executor.h>
#include <algorithm>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("net.LocalConnector", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

// {{{ LocalEndPoint impl
LocalEndPoint::LocalEndPoint(LocalConnector* connector)
    : ByteArrayEndPoint(), connector_(connector) {}

LocalEndPoint::~LocalEndPoint() {
  TRACE("$0 ~LocalEndPoint: connection=$1", this, connection());
}

void LocalEndPoint::close() {
  ByteArrayEndPoint::close();
  connector_->onEndPointClosed(this);
}

template<>
std::string StringUtil::toString(LocalEndPoint* ep) {
  return StringUtil::format("LocalEndPoint[$0]", (void*)ep);
}

// }}}

LocalConnector::LocalConnector(Executor* executor)
    : Connector("local", executor),
      isStarted_(false),
      pendingConnects_(),
      connectedEndPoints_() {
}

LocalConnector::~LocalConnector() {
  //.
}

void LocalConnector::start() {
  isStarted_ = true;
}

void LocalConnector::stop() {
  isStarted_ = false;
}

std::list<RefPtr<EndPoint>> LocalConnector::connectedEndPoints() {
  std::list<RefPtr<EndPoint>> result;

  for (const auto& ep: connectedEndPoints_)
    result.push_back(ep.as<EndPoint>());

  return result;
}

RefPtr<LocalEndPoint> LocalConnector::createClient(
    const std::string& rawRequestMessage) {

  assert(isStarted());

  pendingConnects_.emplace_back(new LocalEndPoint(this));
  RefPtr<LocalEndPoint> endpoint = pendingConnects_.back();
  endpoint->setInput(rawRequestMessage);

  executor()->execute(std::bind((void (LocalConnector::*)()) &LocalConnector::acceptOne, this));

  return endpoint;
}

bool LocalConnector::acceptOne() {
  assert(isStarted());

  if (pendingConnects_.empty()) {
    return false;
  }

  RefPtr<LocalEndPoint> endpoint = pendingConnects_.front();
  pendingConnects_.pop_front();
  connectedEndPoints_.push_back(endpoint);

  auto connection = defaultConnectionFactory()(this, endpoint.get());
  connection->onOpen(false);

  return true;
}

void LocalConnector::onEndPointClosed(LocalEndPoint* endpoint) {
  TRACE("$0 onEndPointClosed: connection=$1, endpoint=$2",
        this, endpoint->connection(), endpoint);

  // try connected endpoints
  auto i = std::find_if(connectedEndPoints_.begin(), connectedEndPoints_.end(),
                        [endpoint](const RefPtr<LocalEndPoint>& ep) {
    return endpoint == ep.get();
  });

  if (i != connectedEndPoints_.end()) {
    connectedEndPoints_.erase(i);
    return;
  }

  // try pending endpoints
  auto k = std::find_if(pendingConnects_.begin(), pendingConnects_.end(),
                        [endpoint](const RefPtr<LocalEndPoint>& ep) {
    return ep.get() == endpoint;
  });

  if (k != pendingConnects_.end()) {
    pendingConnects_.erase(k);
  }
}

template<>
std::string StringUtil::toString(LocalConnector* value) {
  return StringUtil::format("LocalConnector[$0]", (void*)value);
}

}  // namespace xzero
