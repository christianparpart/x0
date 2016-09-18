// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Discovery.h>

namespace xzero {
namespace raft {

// {{{ LocalTransport
LocalTransport::LocalTransport(Id localId)
    : localId_(localId) {
}

void LocalTransport::send(Id target, const VoteRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void LocalTransport::send(Id target, const VoteResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void LocalTransport::send(Id target, const AppendEntriesRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void LocalTransport::send(Id target, const AppendEntriesResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void LocalTransport::send(Id target, const InstallSnapshotRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void LocalTransport::send(Id target, const InstallSnapshotResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}
// }}}

} // namespace raft
} // namespace xzero
