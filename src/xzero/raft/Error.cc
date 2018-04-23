// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/Error.h>
#include <fmt/format.h>

namespace xzero::raft {

RaftCategory& RaftCategory::get() {
  static RaftCategory cat;
  return cat;
}

const char* RaftCategory::name() const noexcept {
  return "Raft";
};

std::string RaftCategory::message(int ec) const {
  switch (static_cast<RaftError>(ec)) {
    case RaftError::Success:
      return "Success";
    case RaftError::MismatchingServerId:
      return "Mismatching server ID";
    case RaftError::NotLeading:
      return "Not leading the cluster";
    case RaftError::CommitTimeout:
      return "Commit timeout";
    case RaftError::ServerNotFound:
      return "Server not found";
    default:
      return fmt::format("RaftError<{}>", ec);
  }
}

} // namespace xzero::raft
