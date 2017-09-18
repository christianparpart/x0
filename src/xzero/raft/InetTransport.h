// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/Transport.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/Connection.h>
#include <xzero/net/InetEndPoint.h>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace xzero {

class Executor;

namespace raft {

class Discovery;
class PeerConnection;

/**
 * Implements Raft peer-to-peer communication over streaming sockets.
 *
 * All public methods are thread safe.
 *
 * All @c send() methods are sending the current message in blocking mode,
 * potentialy reusing a already idle connection that has been pulled from
 * the connection pooa already idle connection that has been pulled from
 * the connection pool.
 *
 * Once the message has been fully sent to the peer, the endpoint is
 * put back into the connection pool, and registered to the non-blocking
 * executor, waiting for input.
 *
 * Once an idle endpoint from the endpoint pool becomes readable,
 * the incoming message is being read in non-blocking and then sent
 * to the @c Handler for further proccessing.
 */
class InetTransport : public Transport {
 public:
  typedef std::function<RefPtr<InetEndPoint>(const std::string&)>
      EndPointCreator;

  InetTransport(const Discovery* discovery,
                Executor* handlerExecutor,
                EndPointCreator endpointCreator,
                std::shared_ptr<InetConnector> connector);

  ~InetTransport();

  // Transport overrides
  void setHandler(Handler* handler) override;
  void send(Id target, const VoteRequest& message) override;
  void send(Id target, const AppendEntriesRequest& message) override;
  void send(Id target, const InstallSnapshotRequest& message) override;

  InetConnector* connector() const { return connector_.get(); }

 private:
  Connection* create(InetConnector* connector, InetEndPoint* endpoint);

 private:
  RefPtr<InetEndPoint> getEndPoint(Id target);
  void watchEndPoint(Id target, RefPtr<InetEndPoint> ep);
  void onClose(Id target);
  friend class PeerConnection;

 private:
  const Discovery* discovery_;
  Handler* handler_;
  Executor* handlerExecutor_;
  EndPointCreator endpointCreator_;
  std::shared_ptr<InetConnector> connector_;

  std::mutex endpointLock_;
  std::unordered_map<Id, RefPtr<InetEndPoint>> endpoints_;
};

} // namespace raft
} // namespace xzero
