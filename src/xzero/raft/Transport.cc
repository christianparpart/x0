// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Transport.h>
#include <xzero/raft/Listener.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Server.h>
#include <xzero/raft/Generator.h>
#include <xzero/raft/Parser.h>
#include <xzero/net/EndPoint.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/net/InetConnector.h>
#include <xzero/BufferUtil.h>
#include <xzero/logging.h>

namespace xzero {
namespace raft {

Transport::~Transport() {
}

// {{{ PeerConnection
class PeerConnection : public Connection {
 public:
  PeerConnection(Connector* connector,
                 EndPoint* endpoint,
                 Id peerId,
                 Listener* listener);

  void onOpen(bool dataReady) override;
  void onFillable() override;
  void onFlushable() override;

 private:
  Buffer inputBuffer_;
  Parser parser_;
};

PeerConnection::PeerConnection(Connector* connector,
                               EndPoint* endpoint,
                               Id peerId,
                               Listener* listener)
  : Connection(endpoint, connector->executor()),
    inputBuffer_(4096),
    parser_(peerId, listener) {
}

void PeerConnection::onOpen(bool dataReady) {
  Connection::onOpen(dataReady);

  if (dataReady) {
    onFillable();
  } else {
    wantFill();
  }
}

void PeerConnection::onFillable() {
  endpoint()->fill(&inputBuffer_);
  parser_.parseFragment(inputBuffer_);
  inputBuffer_.clear();

  endpoint()->wantFill();
}

void PeerConnection::onFlushable() {
}
// }}}

// {{{ InetTransport
InetTransport::InetTransport(Id myId,
                             Listener* receiver,
                             Discovery* discovery,
                             std::unique_ptr<Connector> connector)
  : ConnectionFactory("raft"),
    myId_(myId),
    receiver_(receiver),
    discovery_(discovery),
    connector_(std::move(connector)) {
  if (connector_.get() == nullptr) {
    // TODO: move it all out
    InetAddress myAddr(*discovery_->getAddress(myId_));

    Executor* executor = nullptr; // TDOO

    InetConnector::ExecutorSelector clientExecutorSelector = 
        [executor]() -> Executor* { return executor; };

    InetConnector* inet = new InetConnector("raft-tcp",
                                            executor,
                                            clientExecutorSelector,
                                            5_seconds, // readTimeout,
                                            5_seconds, // writeTimeout,
                                            5_seconds, // tcpFinTimeout,
                                            myAddr.ip(),
                                            myAddr.port(),
                                            128, // backlog,
                                            true, // reuseAddr,
                                            false // reusePort
                                            );
  }
}

InetTransport::~InetTransport() {
}

Connection* InetTransport::create(Connector* connector,
                                  EndPoint* endpoint) {
  Id peerId = 0; // TODO: detect peer ID
  return configure(
      endpoint->setConnection<PeerConnection>(connector,
                                              endpoint,
                                              peerId,
                                              receiver_),
      connector);
}

RefPtr<EndPoint> InetTransport::getEndPoint(Id target) {
  constexpr Duration connectTimeout = 5_seconds;
  constexpr Duration readTimeout = 5_seconds;
  constexpr Duration writeTimeout = 5_seconds;

  Result<std::string> address = discovery_->getAddress(target);
  if (address.isFailure())
    return nullptr;

  // TODO: make ourself independant from TCP/IP here and also allow other EndPoint's.
  // TODO: add endpoint pooling, to reduce resource hogging.
  RefPtr<EndPoint> ep = InetEndPoint::connect(
      InetAddress(*address),
      connectTimeout,
      readTimeout,
      writeTimeout,
      connector_->executor());

  return ep;
}

void InetTransport::send(Id target, const VoteRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateVoteRequest(msg);
    ep->flush(buffer);
  }
}

void InetTransport::send(Id target, const VoteResponse& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateVoteResponse(msg);
    ep->flush(buffer);
  }
}

void InetTransport::send(Id target, const AppendEntriesRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateAppendEntriesRequest(msg);
    ep->flush(buffer);
  }
}

void InetTransport::send(Id target, const AppendEntriesResponse& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateAppendEntriesResponse(msg);
    ep->flush(buffer);
  }
}

void InetTransport::send(Id target, const InstallSnapshotRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateInstallSnapshotRequest(msg);
    ep->flush(buffer);
  }
}

void InetTransport::send(Id target, const InstallSnapshotResponse& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateInstallSnapshotResponse(msg);
    ep->flush(buffer);
  }
}
// }}}

// {{{ LocalTransport
LocalTransport::LocalTransport(Id myId, Executor* executor)
    : myId_(myId),
      executor_(executor),
      peers_() {
}

LocalTransport::LocalTransport(LocalTransport&& m)
    : myId_(m.myId_),
      peers_(std::move(m.peers_)) {
}

LocalTransport& LocalTransport::operator=(LocalTransport&& m) {
  myId_ = std::move(m.myId_);
  peers_ = std::move(m.peers_);
  return *this;
}

void LocalTransport::setPeer(Id peerId, Listener* target) {
  peers_[peerId] = target;
}

void LocalTransport::send(Id target, const VoteRequest& msg) {
  assert(myId_ == msg.candidateId);

  // sends a VoteRequest msg to the given target.
  // XXX emulate async/delayed behaviour by deferring receival via executor API.
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    executor_->execute([=]() {
      i->second->receive(myId_, msg);
    });
    //logDebug("raft.LocalTransport", "$0 send to $1: $2", myId_, target, msg);
  }
}

void LocalTransport::send(Id target, const VoteResponse& msg) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    //logDebug("raft.LocalTransport", "$0 send to $1: $2", myId_, target, msg);
    executor_->execute([=]() {
      i->second->receive(myId_, msg);
    });
  }
}

void LocalTransport::send(Id target, const AppendEntriesRequest& msg) {
  assert(myId_ == msg.leaderId);

  auto i = peers_.find(target);
  if (i != peers_.end()) {
    executor_->execute([=]() {
      logDebug("raft.LocalTransport", "AppendEntriesRequest from $0 to $1: $2", myId_, i->first, msg);
      i->second->receive(myId_, msg);
    });
  }
}

void LocalTransport::send(Id target, const AppendEntriesResponse& msg) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    executor_->execute([=]() {
      logDebug("raft.LocalTransport", "AppendEntriesResponse from $0 to $1: $2", myId_, i->first, msg);
      i->second->receive(myId_, msg);
    });
  }
}

void LocalTransport::send(Id target, const InstallSnapshotRequest& msg) {
  assert(myId_ == msg.leaderId);

  auto i = peers_.find(target);
  if (i != peers_.end()) {
    executor_->execute([=]() {
      i->second->receive(myId_, msg);
    });
  }
}

void LocalTransport::send(Id target, const InstallSnapshotResponse& msg) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    executor_->execute([=]() {
      i->second->receive(myId_, msg);
    });
  }
}
// }}}

} // namespace raft
} // namespace xzero
