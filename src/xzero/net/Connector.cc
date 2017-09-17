// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/Connector.h>
#include <xzero/net/Server.h>
#include <xzero/net/EndPoint.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/Buffer.h>
#include <xzero/BufferUtil.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <algorithm>
#include <cassert>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("Connector", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

Connector::Connector(const std::string& name, Executor* executor)
  : name_(name),
    executor_(executor),
    connectionFactories_(),
    defaultConnectionFactory_() {
}

Connector::~Connector() {
}

const std::string& Connector::name() const {
  return name_;
}

void Connector::setName(const std::string& name) {
  name_ = name;
}

void Connector::addConnectionFactory(const std::string& protocolName,
                                     ConnectionFactory factory) {
  assert(protocolName != "");
  assert(!!factory);

  connectionFactories_[protocolName] = factory;

  if (connectionFactories_.size() == 1) {
    defaultConnectionFactory_ = protocolName;
  }
}

void Connector::createConnection(const std::string& protocolName,
                                            EndPoint* ep) {
  TRACE("createConnection: \"$0\"", protocolName);
  auto factory = connectionFactory(protocolName);
  if (factory) {
    factory(this, ep);
  } else {
    defaultConnectionFactory()(this, ep);
  }
}

Connector::ConnectionFactory Connector::connectionFactory(const std::string& protocolName) const {
  auto i = connectionFactories_.find(protocolName);
  if (i != connectionFactories_.end()) {
    return i->second;
  }
  return nullptr;
}

std::list<std::string> Connector::connectionFactories() const {
  std::list<std::string> result;
  for (auto& entry: connectionFactories_) {
    result.push_back(entry.first);
  }
  return result;
}

size_t Connector::connectionFactoryCount() const {
  return connectionFactories_.size();
}

void Connector::setDefaultConnectionFactory(const std::string& protocolName) {
  auto i = connectionFactories_.find(protocolName);
  if (i == connectionFactories_.end())
    throw std::runtime_error("Invalid argument.");

  defaultConnectionFactory_ = protocolName;
}

Connector::ConnectionFactory Connector::defaultConnectionFactory() const {
  auto i = connectionFactories_.find(defaultConnectionFactory_);
  if (i == connectionFactories_.end())
    RAISE_STATUS(InternalError);

  return i->second;
}

void Connector::loadConnectionFactorySelector(const std::string& protocolName,
                                              Buffer* sink) {
  auto i = connectionFactories_.find(defaultConnectionFactory_);
  if (i == connectionFactories_.end())
    RAISE(InvalidArgumentError, "Invalid protocol name.");

  sink->push_back((char) MagicProtocolSwitchByte);
  BinaryWriter(BufferUtil::writer(sink)).writeString(protocolName);
}

std::string Connector::toString() const {
  char buf[128];
  int n = snprintf(buf, sizeof(buf), "Connector/%s @ %p", name_.c_str(), this);
  return std::string(buf, n);
}

template <>
std::string StringUtil::toString(Connector* connector) {
  return connector->toString();
}

template <>
std::string StringUtil::toString(const Connector* connector) {
  return connector->toString();
}

} // namespace xzero
