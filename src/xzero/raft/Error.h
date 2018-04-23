// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <string>
#include <cstdint>
#include <system_error>
#include <fmt/format.h>

namespace xzero {
namespace raft {

enum class RaftError {
  //! No error has occurred.
  Success = 0,
  //! The underlying storage engine reports a different server ID than supplied.
  MismatchingServerId,
  //! This RaftServer is currently not the leader.
  NotLeading,
  //! Timed out committing the command.
  CommitTimeout,

  //! Server with given ID not found.
  ServerNotFound,
};

class RaftCategory : public std::error_category {
 public:
  static RaftCategory& get();

  const char* name() const noexcept override;
  std::string message(int ec) const override;
};

inline std::error_code make_error_code(RaftError ec) {
  return std::error_code((int) ec, RaftCategory::get());
}

} // namespace raft
} // namespace xzero

namespace std {
  template <>
  struct hash<xzero::raft::RaftError> {
    size_t operator()(xzero::raft::RaftError error) const {
      return static_cast<size_t>(error);
    }
  };

  template<> struct is_error_code_enum<xzero::raft::RaftError> : public true_type{};
}

namespace fmt {
  template<>
  struct formatter<xzero::raft::RaftError> {
    using RaftError = xzero::raft::RaftError;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const RaftError& v, FormatContext &ctx) {
      switch (v) {
        case RaftError::Success: return format_to(ctx.begin(), "Success");
        case RaftError::MismatchingServerId: return format_to(ctx.begin(), "MismatchingServerId");
        case RaftError::NotLeading: return format_to(ctx.begin(), "NotLeading");
        case RaftError::CommitTimeout: return format_to(ctx.begin(), "CommitTimeout");
        case RaftError::ServerNotFound: return format_to(ctx.begin(), "ServerNotFound");
        default: return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}

