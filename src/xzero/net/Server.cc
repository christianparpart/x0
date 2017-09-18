// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/Server.h>
#include <xzero/net/InetConnector.h>
#include <xzero/logging.h>
#include <algorithm>

namespace xzero {

#define TRACE(msg...) logTrace("Server", msg)

Server::Server()
    : connectors_() {
}

Server::Server(int port)
    : connectors_() {
  TRACE("TODO addConnector<InetConnector>(IPAddress(\"0.0.0.0\"), port);");
}

Server::Server(const IPAddress& address, int port)
    : connectors_() {
  TRACE("TODO addConnector<InetConnector>(address, port);");
}

Server::~Server() {
  for (InetConnector* connector : connectors_) {
    delete connector;
  }
}

void Server::start() {
  for (InetConnector* connector : connectors_) {
    connector->start();
  }
}

void Server::stop() {
  for (InetConnector* connector : connectors_) {
    connector->stop();
  }
}

void Server::implAddConnector(InetConnector* connector) {
  connectors_.push_back(connector);
}

void Server::removeAllConnectors() {
  while (!connectors_.empty()) {
    removeConnector(connectors_.back());
  }
}

void Server::removeConnector(InetConnector* connector) {
  auto i = std::find(connectors_.begin(), connectors_.end(), connector);
  if (i != connectors_.end()) {
    TRACE("removing connector $0", connector);
    connectors_.erase(i);
    delete connector;
  }
}

std::list<InetConnector*> Server::getConnectors() const {
  return connectors_;
}

std::list<InetConnector*> Server::findConnectors(const IPAddress& ip, int port) {
  std::list<InetConnector*> result;

  for (InetConnector* inet: connectors_)
    if (inet->bindAddress() == ip && inet->port() == port)
      result.push_back(inet);

  return result;
}

}  // namespace xzero
