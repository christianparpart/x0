// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Transport.h>
#include <xzero/raft/Handler.h>
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

/*
 * - the peer that wants to say something, initiates the connection
 * - the connection may be reused (pooled) for future messages
 * - an incoming connection MAY be reused to send the response, too
 */

// {{{ PeerConnection
class PeerConnection
  : public Connection,
    public Listener {
 public:
  PeerConnection(Connector* connector,
                 EndPoint* endpoint,
                 Id peerId,
                 Handler* handler);

  // Connection override (connection-endpoint hooks)
  void onOpen(bool dataReady) override;
  void onFillable() override;
  void onFlushable() override;

  // Listener overrides (parser hooks)
  void receive(Id from, const VoteRequest& message) override;
  void receive(Id from, const VoteResponse& message) override;
  void receive(Id from, const AppendEntriesRequest& message) override;
  void receive(Id from, const AppendEntriesResponse& message) override;
  void receive(Id from, const InstallSnapshotResponse& message) override;
  void receive(Id from, const InstallSnapshotRequest& message) override;

 private:
  Buffer inputBuffer_;
  Buffer outputBuffer_;
  size_t outputOffset_;
  Handler* handler_;
  Parser parser_;
};

PeerConnection::PeerConnection(Connector* connector,
                               EndPoint* endpoint,
                               Id peerId,
                               Handler* handler)
  : Connection(endpoint, connector->executor()),
    inputBuffer_(4096),
    outputBuffer_(4096),
    outputOffset_(0),
    handler_(handler),
    parser_(peerId, this) {
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

void PeerConnection::receive(Id from, const VoteRequest& req) {
  VoteResponse res = handler_->handleRequest(from, req);
  Generator(BufferUtil::writer(&outputBuffer_)).generateVoteResponse(res);
}

void PeerConnection::receive(Id from, const VoteResponse& res) {
  handler_->handleResponse(from, res);
}

void PeerConnection::receive(Id from, const AppendEntriesRequest& req) {
  AppendEntriesResponse res = handler_->handleRequest(from, req);
  Generator(BufferUtil::writer(&outputBuffer_)).generateAppendEntriesResponse(res);
}

void PeerConnection::receive(Id from, const AppendEntriesResponse& res) {
  handler_->handleResponse(from, res);
}

void PeerConnection::receive(Id from, const InstallSnapshotRequest& req) {
  InstallSnapshotResponse res = handler_->handleRequest(from, req);
  Generator(BufferUtil::writer(&outputBuffer_)).generateInstallSnapshotResponse(res);
}

void PeerConnection::receive(Id from, const InstallSnapshotResponse& res) {
  handler_->handleResponse(from, res);
}
// }}}

// {{{ InetTransport
InetTransport::InetTransport(const Discovery* discovery,
                             Executor* handlerExecutor,
                             std::shared_ptr<Connector> connector)
  : ConnectionFactory("raft"),
    discovery_(discovery),
    handler_(nullptr),
    handlerExecutor_(handlerExecutor),
    connector_(connector) {
}

InetTransport::~InetTransport() {
}

void InetTransport::setHandler(Handler* handler) {
  handler_ = handler;
}

Connection* InetTransport::create(Connector* connector,
                                  EndPoint* endpoint) {
  Id peerId = 0;
  Option<InetAddress> remoteAddress = endpoint->remoteAddress();
  if (remoteAddress.isSome()) {
    std::string remoteAddrStr = StringUtil::toString(*remoteAddress);
    Result<Id> peerIdResult = discovery_->getId(remoteAddrStr);
    if (peerIdResult.isSuccess()) {
      peerId = *peerIdResult;
    }
  }

  return configure(
      endpoint->setConnection<PeerConnection>(connector,
                                              endpoint,
                                              peerId,
                                              handler_),
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

void InetTransport::send(Id target, const AppendEntriesRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateAppendEntriesRequest(msg);
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
// }}}

// {{{ LocalTransport
LocalTransport::LocalTransport(Id myId, Executor* executor)
    : myId_(myId),
      myHandler_(nullptr),
      executor_(executor),
      peers_() {
}

LocalTransport::LocalTransport(LocalTransport&& m)
    : myId_(m.myId_),
      myHandler_(m.myHandler_),
      executor_(m.executor_),
      peers_(std::move(m.peers_)) {
}

LocalTransport& LocalTransport::operator=(LocalTransport&& m) {
  myId_ = std::move(m.myId_);
  myHandler_ = std::move(m.myHandler_);
  executor_ = std::move(m.executor_);
  peers_ = std::move(m.peers_);
  return *this;
}

void LocalTransport::setPeer(Id peerId, Handler* target) {
  peers_[peerId] = target;
}

Handler* LocalTransport::getPeer(Id id) {
  auto i = peers_.find(id);
  if (i != peers_.end()) {
    return i->second;
  } else {
    return nullptr;
  }
}

void LocalTransport::setHandler(Handler* handler) {
  myHandler_ = handler;
}

void LocalTransport::send(Id target, const VoteRequest& msg) {
  assert(msg.candidateId == myId_);

  // sends a VoteRequest msg to the given target.
  // XXX emulate async/delayed behaviour by deferring receival via executor API.
  executor_->execute([=]() {
    VoteResponse result = getPeer(target)->handleRequest(myId_, msg);
    executor_->execute([=]() {
      getPeer(myId_)->handleResponse(target, result);
    });
  });
  //logDebug("raft.LocalTransport", "$0 send to $1: $2", myId_, target, msg);
}


void LocalTransport::send(Id target, const AppendEntriesRequest& msg) {
  assert(msg.leaderId == myId_);

  executor_->execute([=]() {
    logDebug("raft.LocalTransport", "AppendEntriesRequest from $0 to $1: $2", myId_, target, msg);
    AppendEntriesResponse result = getPeer(target)->handleRequest(myId_, msg);
    executor_->execute([=]() {
      getPeer(myId_)->handleResponse(target, result);
    });
  });
}

void LocalTransport::send(Id target, const InstallSnapshotRequest& msg) {
  assert(msg.leaderId == myId_);

  executor_->execute([=]() {
    InstallSnapshotResponse result = getPeer(target)->handleRequest(myId_, msg);
    executor_->execute([=]() {
      getPeer(myId_)->handleResponse(target, result);
    });
  });
}
// }}}

} // namespace raft
} // namespace xzero
