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

class Buffer;
class Executor;
class WallClock;
class EndPoint;
class Connection;

/**
 * Base API for accepting new clients and binding them to a Connection.
 *
 * @see EndPoint
 * @see Connection
 */
class XZERO_BASE_API Connector {
 public:
  //! Must be a non-printable ASCII byte.
  enum { MagicProtocolSwitchByte = 0x01 };

  /**
   * Creates a new Connection instance for the given @p connector
   * and @p endpoint.
   *
   * @param connector the Connector that accepted the incoming connection.
   * @param endpoint the endpoint that corresponds to this connection.
   *
   * @return pointer to the newly created Connection instance.
   *
   * The newly created Connection instance will be owned by its EndPoint.
   */
  typedef std::function<Connection*(Connector*, EndPoint*)> ConnectionFactory;

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
  void addConnectionFactory(const std::string& protocol, ConnectionFactory factory);

  /**
   * Retrieves associated connection factory by @p protocolName.
   *
   * @param protocolName protocol name for the connection factory to retrieve.
   */
  ConnectionFactory connectionFactory(const std::string& protocolName) const;

  /** Retrieves all registered connection factories. */
  std::list<std::string> connectionFactories() const;

  /** Retrieves number of registered connection factories. */
  size_t connectionFactoryCount() const;

  /**
   * Sets the default connection factory.
   */
  void setDefaultConnectionFactory(const std::string& protocolName);

  /**
   * Retrieves the default connection factory.
   */
  ConnectionFactory defaultConnectionFactory() const;

  void loadConnectionFactorySelector(const std::string& protocolName, Buffer* sink);

  /**
   * Retrieves the default task executor service.
   */
  Executor* executor() const { return executor_; }

  virtual std::string toString() const;

 private:
  std::string name_;
  Executor* executor_;

  std::unordered_map<std::string, ConnectionFactory> connectionFactories_;
  std::string defaultConnectionFactory_;
};

}  // namespace xzero
