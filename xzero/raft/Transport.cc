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
 * - an incoming connection MUST be reused to send the corresponding response
 */

// {{{ PeerConnection
/**
 * PeerConnection represents an incoming peer commection.
 */
class PeerConnection
  : public Connection,
    public Listener {
 public:
  PeerConnection(Connector* connector,
                 EndPoint* endpoint,
                 Handler* handler);

  // Connection override (connection-endpoint hooks)
  void onOpen(bool dataReady) override;
  void onFillable() override;
  void onFlushable() override;

  // Listener overrides (parser hooks)
  void receive(const HelloRequest& message) override;
  void receive(const HelloResponse& message) override;
  void receive(const VoteRequest& message) override;
  void receive(const VoteResponse& message) override;
  void receive(const AppendEntriesRequest& message) override;
  void receive(const AppendEntriesResponse& message) override;
  void receive(const InstallSnapshotResponse& message) override;
  void receive(const InstallSnapshotRequest& message) override;

 private:
  Id peerId_;
  Buffer inputBuffer_;
  Buffer outputBuffer_;
  size_t outputOffset_;
  Handler* handler_;
  Parser parser_;
};

PeerConnection::PeerConnection(Connector* connector,
                               EndPoint* endpoint,
                               Handler* handler)
  : Connection(endpoint, connector->executor()),
    peerId_(0),
    inputBuffer_(4096),
    outputBuffer_(4096),
    outputOffset_(0),
    handler_(handler),
    parser_(this) {
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

void PeerConnection::receive(const HelloRequest& req) {
  HelloResponse res = handler_->handleRequest(req);
  Generator(BufferUtil::writer(&outputBuffer_)).generateHelloResponse(res);

  if (res.success) {
    peerId_ = req.serverId;
  }
}

void PeerConnection::receive(const HelloResponse& res) {
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
  return configure(
      endpoint->setConnection<PeerConnection>(connector,
                                              endpoint,
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
    consumeResponse(target, ep);
  }
}

void InetTransport::send(Id target, const AppendEntriesRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateAppendEntriesRequest(msg);
    ep->flush(buffer);
    consumeResponse(target, ep);
  }
}

void InetTransport::send(Id target, const InstallSnapshotRequest& msg) {
  if (RefPtr<EndPoint> ep = getEndPoint(target)) {
    Buffer buffer;
    Generator(BufferUtil::writer(&buffer)).generateInstallSnapshotRequest(msg);
    ep->flush(buffer);
    consumeResponse(target, ep);
  }
}

void InetTransport::consumeResponse(Id target, RefPtr<EndPoint> ep) {
  // TODO
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
    logError("raft", "LocalTransport($0).getPeer($1) failed.", myId_, id);
    RAISE_ERROR(std::make_error_code(RaftError::ServerNotFound));
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
    //logDebug("raft.LocalTransport", "$0-to-$1: recv $2", myId_, target, msg);
    AppendEntriesResponse result = getPeer(target)->handleRequest(myId_, msg);
    executor_->execute([=]() {
      //logDebug("raft.LocalTransport", "$0-to-$1: recv $2", target, myId_, result);
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
