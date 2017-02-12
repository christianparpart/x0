// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/MessageType.h>
#include <xzero/StringUtil.h>

namespace xzero {

using raft::MessageType;

template<>
std::string StringUtil::toString<>(const MessageType type) {
  switch (type) {
    case MessageType::VoteRequest:
      return "VoteRequest";
    case MessageType::VoteResponse:
      return "VoteResponse";
    case MessageType::AppendEntriesRequest:
      return "AppendEntriesRequest";
    case MessageType::AppendEntriesResponse:
      return "AppendEntriesResponse";
    case MessageType::InstallSnapshotRequest:
      return "InstallSnapshotRequest";
    case MessageType::InstallSnapshotResponse:
      return "InstallSnapshotResponse";
    case MessageType::HelloRequest:
      return "HelloRequest";
    case MessageType::HelloResponse:
      return "HelloResponse";
    default: {
      char buf[5];
      snprintf(buf, sizeof(buf), "0x%02x", (unsigned) type);
      return std::string(buf);
    }
  }
}

} // namespace xzero
