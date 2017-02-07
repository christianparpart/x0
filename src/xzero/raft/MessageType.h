// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <cstdint>

namespace xzero {
namespace raft {

enum class MessageType : uint8_t {
  VoteRequest = 1,
  VoteResponse = 2,
  AppendEntriesRequest = 3,
  AppendEntriesResponse = 4,
  InstallSnapshotRequest = 5,
  InstallSnapshotResponse = 6,
  HelloRequest = 7,
  HelloResponse = 8,
};

} // namespace raft
} // namespace xzero
