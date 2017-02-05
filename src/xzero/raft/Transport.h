// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>
#include <xzero/net/Connector.h>
#include <xzero/net/Connection.h>
#include <xzero/net/ConnectionFactory.h>
#include <xzero/net/EndPoint.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace xzero {

class Executor;

namespace raft {

class Handler;
class Discovery;

/**
 * Abstracts peer-to-peer communication between Server instances.
 */
class Transport {
 public:
  virtual ~Transport();

  virtual void setHandler(Handler* handler) = 0;

  virtual void send(Id target, const VoteRequest& message) = 0;
  virtual void send(Id target, const AppendEntriesRequest& message) = 0;
  virtual void send(Id target, const InstallSnapshotRequest& message) = 0;
};

/**
 * Implements Raft peer-to-peer communication over streaming sockets.
 *
 * I/O is implemented non-blocking, and thus, all handler tasks MUST
 * be invoked within a seperate threaded worker executor.
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

 private:
  const Discovery* discovery_;
  Handler* handler_;
  Executor* handlerExecutor_;
  std::shared_ptr<Connector> connector_;
};

/**
 * An in-memory peer-to-peer transport layer (for debugging / unit testing purposes only).
 */
class LocalTransport : public Transport {
 public:
  LocalTransport(Id myId, Executor* executor);

  LocalTransport(const LocalTransport&) = delete;
  LocalTransport& operator=(const LocalTransport&) = delete;

  LocalTransport(LocalTransport&&);
  LocalTransport& operator=(LocalTransport&&);

  void setPeer(Id id, Handler* handler);
  Handler* getPeer(Id id);

  void setHandler(Handler* handler) override;
  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;

 private:
  Id myId_;
  Handler* myHandler_;
  Executor* executor_;
  std::unordered_map<Id, Handler*> peers_;
};

} // namespace raft
} // namespace xzero
