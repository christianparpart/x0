// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <format>

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

template<>
struct std::formatter<xzero::raft::MessageType> {
  using MessageType = xzero::raft::MessageType;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const MessageType& t, std::format_context& ctx) const {
    switch (t) {
      case MessageType::VoteRequest: return std::format_to(ctx.out(), "VoteRequest");
      case MessageType::VoteResponse: return std::format_to(ctx.out(), "VoteResponse");
      case MessageType::AppendEntriesRequest: return std::format_to(ctx.out(), "AppendEntriesRequest");
      case MessageType::AppendEntriesResponse: return std::format_to(ctx.out(), "AppendEntriesResponse");
      case MessageType::InstallSnapshotRequest: return std::format_to(ctx.out(), "InstallSnapshotRequest");
      case MessageType::InstallSnapshotResponse: return std::format_to(ctx.out(), "InstallSnapshotResponse");
      case MessageType::HelloRequest: return std::format_to(ctx.out(), "HelloRequest");
      case MessageType::HelloResponse: return std::format_to(ctx.out(), "HelloResponse");
      default: return std::format_to(ctx.out(), "({})", (int) t);
    }
  }
};

