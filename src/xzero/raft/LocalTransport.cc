// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/LocalTransport.h>
#include <xzero/raft/Handler.h>
#include <xzero/raft/Listener.h>
#include <xzero/raft/Discovery.h>
#include <xzero/raft/Server.h>
#include <xzero/raft/Generator.h>
#include <xzero/raft/Parser.h>
#include <xzero/BufferUtil.h>
#include <xzero/logging.h>

namespace xzero {
namespace raft {
/*
 * - the peer that wants to say something, initiates the connection
 * - the connection may be reused (pooled) for future messages
 * - an incoming connection MUST be reused to send the corresponding response
 */

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

} // namespace raft
} // namespace xzero
