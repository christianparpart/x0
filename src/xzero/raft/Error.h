// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <string>
#include <cstdint>
#include <system_error>

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
struct hash<xzero::raft::RaftError> : public unary_function<xzero::raft::RaftError, size_t> {
  size_t operator()(xzero::raft::RaftError error) const {
    return (size_t) error;
  }
};

template<> struct is_error_code_enum<xzero::raft::RaftError> : public true_type{};

}
