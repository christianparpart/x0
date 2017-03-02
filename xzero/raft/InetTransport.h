// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/Transport.h>
#include <xzero/net/Connector.h>
#include <xzero/net/Connection.h>
#include <xzero/net/ConnectionFactory.h>
#include <xzero/net/EndPoint.h>
#include <memory>

namespace xzero {

class Executor;

namespace raft {

class Discovery;

/**
 * Implements Raft peer-to-peer communication over streaming sockets.
 */
class InetTransport
  : public Transport,
    public ConnectionFactory {
 public:
  InetTransport(const Discovery* discovery,
                Executor* handlerExecutor,
                std::shared_ptr<Connector> connector);

  ~InetTransport();

  // Transport overrides
  void setHandler(Handler* handler) override;
  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;

  // ConnectionFactory overrides
  Connection* create(Connector* connector, EndPoint* endpoint) override;

  Connector* connector() const { return connector_.get(); }

 private:
  RefPtr<EndPoint> getEndPoint(Id target);
  void consumeResponse(Id target, RefPtr<EndPoint> ep);

 private:
  const Discovery* discovery_;
  Handler* handler_;
  Executor* handlerExecutor_;
  std::shared_ptr<Connector> connector_;
};

} // namespace raft
} // namespace xzero
