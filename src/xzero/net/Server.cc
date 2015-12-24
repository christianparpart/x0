// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/Server.h>
#include <xzero/net/Connector.h>
#include <xzero/net/InetConnector.h>
#include <xzero/logging.h>
#include <algorithm>

namespace xzero {

#define TRACE(msg...) logTrace("Server", msg)

Server::Server()
    : connectors_(), date_() {
}

Server::Server(int port)
    : connectors_(), date_() {
  TRACE("TODO addConnector<InetConnector>(IPAddress(\"0.0.0.0\"), port);");
}

Server::Server(const IPAddress& address, int port)
    : connectors_(), date_() {
  TRACE("TODO addConnector<InetConnector>(address, port);");
}

Server::~Server() {
  for (Connector* connector : connectors_) {
    delete connector;
  }
}

void Server::start() {
  for (Connector* connector : connectors_) {
    connector->start();
  }
}

void Server::stop() {
  for (Connector* connector : connectors_) {
    connector->stop();
  }
}

void Server::implAddConnector(Connector* connector) {
  connector->setServer(this);
  connectors_.push_back(connector);
}

void Server::removeAllConnectors() {
  while (!connectors_.empty()) {
    removeConnector(connectors_.back());
  }
}

void Server::removeConnector(Connector* connector) {
  auto i = std::find(connectors_.begin(), connectors_.end(), connector);
  if (i != connectors_.end()) {
    TRACE("removing connector $0", connector);
    connectors_.erase(i);
    delete connector;
  }
}

std::list<Connector*> Server::getConnectors() const {
  return connectors_;
}

std::list<Connector*> Server::findConnectors(const IPAddress& ip, int port) {
  std::list<Connector*> result;

  for (Connector* connector: connectors_)
    if (auto inet = dynamic_cast<InetConnector*>(connector))
      if (inet->bindAddress() == ip && inet->port() == port)
        result.push_back(inet);

  return result;
}

void Server::getDate(char* buf, size_t size) {
  if (size) {
    size_t n = std::min(size, date_.size()) - 1;
    memcpy(buf, date_.c_str(), n);
    buf[n] = '\0';
  }
}

}  // namespace xzero
