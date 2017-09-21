// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/MessageType.h>
#include <xzero/StringUtil.h>
#include <iostream>

namespace xzero::raft {

std::ostream& operator<<(std::ostream& os, MessageType type) {
  switch (type) {
    case MessageType::VoteRequest:
      return os << "VoteRequest";
    case MessageType::VoteResponse:
      return os << "VoteResponse";
    case MessageType::AppendEntriesRequest:
      return os << "AppendEntriesRequest";
    case MessageType::AppendEntriesResponse:
      return os << "AppendEntriesResponse";
    case MessageType::InstallSnapshotRequest:
      return os << "InstallSnapshotRequest";
    case MessageType::InstallSnapshotResponse:
      return os << "InstallSnapshotResponse";
    case MessageType::HelloRequest:
      return os << "HelloRequest";
    case MessageType::HelloResponse:
      return os << "HelloResponse";
    default: {
      char buf[5];
      snprintf(buf, sizeof(buf), "0x%02x", (unsigned) type);
      return os << std::string(buf);
    }
  }
}

} // namespace xzero::raft
