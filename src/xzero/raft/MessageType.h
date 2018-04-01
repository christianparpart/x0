// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <iosfwd>

namespace xzero::raft {

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

} // namespace xzero::raft

namespace fmt {
  template<>
  struct formatter<xzero::raft::MessageType> {
    using MessageType = xzero::raft::MessageType;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const MessageType& t, FormatContext &ctx) {
      switch (t) {
        case MessageType::VoteRequest: return format_to(ctx.begin(), "VoteRequest");
        case MessageType::VoteResponse: return format_to(ctx.begin(), "VoteResponse");
        case MessageType::AppendEntriesRequest: return format_to(ctx.begin(), "AppendEntriesRequest");
        case MessageType::AppendEntriesResponse: return format_to(ctx.begin(), "AppendEntriesResponse");
        case MessageType::InstallSnapshotRequest: return format_to(ctx.begin(), "InstallSnapshotRequest");
        case MessageType::InstallSnapshotResponse: return format_to(ctx.begin(), "InstallSnapshotResponse");
        case MessageType::HelloRequest: return format_to(ctx.begin(), "HelloRequest");
        case MessageType::HelloResponse: return format_to(ctx.begin(), "HelloResponse");
        default: return format_to(ctx.begin(), "({})", (int) t);
      }
    }
  };
}

