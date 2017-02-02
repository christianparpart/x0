// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/raft/rpc.h>

namespace xzero {
namespace raft {

class Handler {
 public:
  virtual ~Handler() {}

  virtual void handleRequest(
      Id from,
      const VoteRequest& request,
      VoteResponse* response) = 0;

  virtual void handleResponse(
      Id from,
      const VoteResponse& response) = 0;

  virtual void handleRequest(
      Id from,
      const AppendEntriesRequest& request,
      AppendEntriesResponse* response) = 0;

  virtual void handleResponse(
      Id from,
      const AppendEntriesResponse& response) = 0;

  virtual void handleRequest(
      Id from,
      const InstallSnapshotRequest& request,
      InstallSnapshotResponse* response) = 0;

  virtual void handleResponse(
      Id from,
      const InstallSnapshotResponse& response) = 0;
};


} // namespace raft
} // namespace xzero
