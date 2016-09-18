// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
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
};

class RaftCategory : public std::error_category {
 public:
  static RaftCategory& get();

  const char* name() const noexcept override;
  std::string message(int ec) const override;
};

} // namespace raft
} // namespace xzero
