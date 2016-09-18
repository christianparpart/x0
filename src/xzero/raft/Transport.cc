// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Transport.h>
#include <xzero/raft/Listener.h>
#include <xzero/raft/Server.h>

namespace xzero {
namespace raft {

Transport::~Transport() {
}

// {{{ LocalTransport
LocalTransport::LocalTransport(Id myId)
    : myId_(myId) {
}

void LocalTransport::setPeer(Id peerId, Listener* target) {
  peers_[peerId] = target;
}

void LocalTransport::send(Id target, const VoteRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(myId_, message);
  }
}

void LocalTransport::send(Id target, const VoteResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(myId_, message);
  }
}

void LocalTransport::send(Id target, const AppendEntriesRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(myId_, message);
  }
}

void LocalTransport::send(Id target, const AppendEntriesResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(myId_, message);
  }
}

void LocalTransport::send(Id target, const InstallSnapshotRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(myId_, message);
  }
}

void LocalTransport::send(Id target, const InstallSnapshotResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(myId_, message);
  }
}
// }}}

} // namespace raft
} // namespace xzero
