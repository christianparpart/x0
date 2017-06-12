// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/InetTransport.h>
#include <xzero/raft/Handler.h>
#include <xzero/raft/Listener.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Server.h>
#include <xzero/raft/Generator.h>
#include <xzero/raft/Parser.h>
#include <xzero/net/EndPoint.h>
#include <xzero/BufferUtil.h>
#include <xzero/logging.h>

namespace xzero {
namespace raft {

/**
 * PeerConnection represents peer commection for InetTransport.
 *
 * Reading is performed non-blocking whereas writing is performed blocking.
 */
class PeerConnection
  : public Connection,
    public Listener {
 public:
  PeerConnection(InetTransport* mgr,
                 Executor* executor,
                 Handler* handler,
                 EndPoint* endpoint,
                 Id peerId = 0);
  ~PeerConnection();

  // Connection override (connection-endpoint hooks)
  void onOpen(bool dataReady) override;
  void onFillable() override;
  void onFlushable() override;

  // Listener overrides for parser
  void receive(const HelloRequest& message) override;
  void receive(const HelloResponse& message) override;
  void receive(const VoteRequest& message) override;
  void receive(const VoteResponse& message) override;
  void receive(const AppendEntriesRequest& message) override;
  void receive(const AppendEntriesResponse& message) override;
  void receive(const InstallSnapshotResponse& message) override;
  void receive(const InstallSnapshotRequest& message) override;

 private:
  InetTransport* manager_;
  Id peerId_;
  Buffer inputBuffer_;
  Buffer outputBuffer_;
  size_t outputOffset_;
  Handler* handler_;
  Parser parser_;
};

PeerConnection::PeerConnection(InetTransport* manager,
                               Executor* executor,
                               Handler* handler,
                               EndPoint* endpoint,
                               Id peerId)
  : Connection(endpoint, executor),
    manager_(manager),
    peerId_(peerId),
    inputBuffer_(4096),
    outputBuffer_(4096),
    outputOffset_(0),
    handler_(handler),
    parser_(this) {
}

PeerConnection::~PeerConnection() {
  if (peerId_ != 0) {
    manager_->onClose(peerId_);
  }
}

void PeerConnection::onOpen(bool dataReady) {
  Connection::onOpen(dataReady);

  if (peerId_ == 0) {
    // XXX this is an incoming connection.
    if (dataReady) {
      onFillable();
    } else {
      wantFill();
    }
  }
}

void PeerConnection::onFillable() {
  size_t n = endpoint()->fill(&inputBuffer_);
  if (n == 0) {
    close();
    return; // EOF
  }

  n = parser_.parseFragment(inputBuffer_);
  inputBuffer_.clear();

  if (n == 0) {
    // no message passed => need more input
    wantFill();
  } else if (outputOffset_ < outputBuffer_.size()) {
    wantFlush();
  }
}

void PeerConnection::onFlushable() {
  size_t n = endpoint()->flush(outputBuffer_.ref(outputOffset_));
  outputOffset_ += n;
  if (outputOffset_ < outputBuffer_.size()) {
    wantFlush();
  } else {
    outputBuffer_.clear();
    outputOffset_ = 0;
    wantFill();
  }
}

void PeerConnection::receive(const HelloRequest& req) {
  HelloResponse res = handler_->handleRequest(req);
  Generator(BufferUtil::writer(&outputBuffer_)).generateHelloResponse(res);

  if (res.success) {
    peerId_ = req.serverId;
  }
}

void PeerConnection::receive(const HelloResponse& res) {
  handler_->handleResponse(peerId_, res);
}

void PeerConnection::receive(const VoteRequest& req) {
  if (peerId_ == 0) {
    close();
  } else {
    VoteResponse res = handler_->handleRequest(peerId_, req);
    Generator(BufferUtil::writer(&outputBuffer_)).generateVoteResponse(res);
  }
}

void PeerConnection::receive(const VoteResponse& res) {
  if (peerId_ == 0) {
    close();
  } else {
    handler_->handleResponse(peerId_, res);
  }
}

void PeerConnection::receive(const AppendEntriesRequest& req) {
  if (peerId_ == 0) {
    close();
  } else {
    AppendEntriesResponse res = handler_->handleRequest(peerId_, req);
    Generator(BufferUtil::writer(&outputBuffer_)).generateAppendEntriesResponse(res);
  }
}

void PeerConnection::receive(const AppendEntriesResponse& res) {
  if (peerId_ == 0) {
    close();
  } else {
    handler_->handleResponse(peerId_, res);
  }
}

void PeerConnection::receive(const InstallSnapshotRequest& req) {
  if (peerId_ == 0) {
    close();
  } else {
    InstallSnapshotResponse res = handler_->handleRequest(peerId_, req);
    Generator(BufferUtil::writer(&outputBuffer_)).generateInstallSnapshotResponse(res);
  }
}

void PeerConnection::receive(const InstallSnapshotResponse& res) {
  if (peerId_ == 0) {
    close();
  } else {
    handler_->handleResponse(peerId_, res);
  }
}

// ----------------------------------------------------------------------------

InetTransport::InetTransport(const Discovery* discovery,
                             Executor* handlerExecutor,
                             EndPointCreator endpointCreator,
                             std::shared_ptr<Connector> connector)
  : discovery_(discovery),
    handler_(nullptr),
    handlerExecutor_(handlerExecutor),
    endpointCreator_(endpointCreator),
    connector_(connector),
    endpointLock_(),
    endpoints_() {
  connector_->addConnectionFactory(
      protocolName(),
      std::bind(&InetTransport::create, this,
                std::placeholders::_1,
                std::placeholders::_2));
}

InetTransport::~InetTransport() {
}

void InetTransport::setHandler(Handler* handler) {
  handler_ = handler;
}

Connection* InetTransport::create(Connector* connector,
                                  EndPoint* endpoint) {
  return endpoint->setConnection<PeerConnection>(this,
                                                 connector->executor(),
                                                 handler_,
                                                 endpoint);
}

RefPtr<EndPoint> InetTransport::getEndPoint(Id target) {
  {
    std::lock_guard<decltype(endpointLock_)> lk(endpointLock_);
    auto i = endpoints_.find(target);
    if (i != endpoints_.end()) {
      RefPtr<EndPoint> ep = i->second;
      endpoints_.erase(i);
      ep->setBlocking(false);
      return ep;
    }
  }

  Result<std::string> address = discovery_->getAddress(target);
  if (address.isFailure())
    return nullptr;

  RefPtr<EndPoint> ep = endpointCreator_(*address);
  if (ep) {
    ep->setConnection<PeerConnection>(this,
                                      handlerExecutor_,
                                      handler_,
                                      ep.get(),
                                      target);
  }

  return ep;
}

void InetTransport::watchEndPoint(Id target, RefPtr<EndPoint> ep) {
  std::lock_guard<decltype(endpointLock_)> lk(endpointLock_);

  endpoints_[target] = ep;
  ep->setBlocking(false);
  ep->wantFill();
}

void InetTransport::onClose(Id target) {
  std::lock_guard<decltype(endpointLock_)> lk(endpointLock_);

  auto i = endpoints_.find(target);
  if (i != endpoints_.end()) {
    endpoints_.erase(i);
  }
}

void InetTransport::send(Id target, const VoteRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateVoteRequest(msg);
    ep->flush(buffer);
    watchEndPoint(target, ep);
  }
}

void InetTransport::send(Id target, const AppendEntriesRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateAppendEntriesRequest(msg);
    ep->flush(buffer);
    watchEndPoint(target, ep);
  }
}

void InetTransport::send(Id target, const InstallSnapshotRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateInstallSnapshotRequest(msg);
    ep->flush(buffer);
    watchEndPoint(target, ep);
  }
}

} // namespace raft
} // namespace xzero
