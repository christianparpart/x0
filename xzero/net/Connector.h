// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RefPtr.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <list>

namespace xzero {

class Executor;
class WallClock;
class EndPoint;
class ConnectionFactory;
class ConnectionListener;

/**
 * Base API for accepting new clients and binding them to a Connection.
 *
 * @see EndPoint
 * @see Connection
 * @see ConnectionFactory
 */
class XZERO_BASE_API Connector {
 public:
  /**
   * Initializes this connector.
   */
  Connector(const std::string& name, Executor* executor);

  virtual ~Connector();

  /**
   * Retrieves the describing name for this connector.
   */
  const std::string& name() const;

  /**
   * Sets a descriptive connector name.
   */
  void setName(const std::string& name);

  /**
   * Starts given connector.
   *
   * @throw std::runtime_error on runtime errors
   */
  virtual void start() = 0;

  /**
   * Tests whether this connector has been started.
   */
  virtual bool isStarted() const XZERO_NOEXCEPT = 0;

  /**
   * Stops given connector.
   */
  virtual void stop() = 0;

  /**
   * Retrieves list of currently connected endpoints.
   */
  virtual std::list<RefPtr<EndPoint>> connectedEndPoints() = 0;

  /**
   * Registeres a new connection factory.
   */
  ConnectionFactory* addConnectionFactory(ConnectionFactory* factory);

  /**
   * Registeres a new connection factory.
   */
  template <typename T, typename... Args>
  std::unique_ptr<T> addConnectionFactory(Args... args);

  /**
   * Retrieves associated connection factory by @p protocolName.
   *
   * @param protocolName protocol name for the connection factory to retrieve.
   */
  ConnectionFactory* connectionFactory(const std::string& protocolName) const;

  /** Retrieves all registered connection factories. */
  std::list<ConnectionFactory*> connectionFactories() const;

  /** Retrieves number of registered connection factories. */
  size_t connectionFactoryCount() const;

  /**
   * Sets the default connection factory.
   */
  void setDefaultConnectionFactory(ConnectionFactory* factory);

  /**
   * Retrieves the default connection factory.
   */
  ConnectionFactory* defaultConnectionFactory() const;

  /**
   * Retrieves the default task executor service.
   */
  Executor* executor() const { return executor_; }

  /**
   * Adds an Connection listener that will be
   * automatically associated to newly created connections.
   */
  void addListener(ConnectionListener* listener);

  /** Retrieves list of connection listeners. */
  const std::list<ConnectionListener*>& listeners() const { return listeners_; }

  virtual std::string toString() const;

 private:
  std::string name_;
  Executor* executor_;

  std::unordered_map<std::string, ConnectionFactory*> connectionFactories_;

  ConnectionFactory* defaultConnectionFactory_;

  std::list<ConnectionListener*> listeners_;
};

template <typename T, typename... Args>
inline std::unique_ptr<T> Connector::addConnectionFactory(Args... args) {
  std::unique_ptr<T> cf = std::make_unique<T>(args...);
  addConnectionFactory(cf.get());
  return cf;
}

}  // namespace xzero
