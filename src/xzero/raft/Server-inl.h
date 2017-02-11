// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/raft/Storage.h>

namespace xzero {
namespace raft {

inline Term Server::currentTerm() const {
  return storage_->currentTerm();
}

inline Id Server::currentLeaderId() const {
  return currentLeaderId_;
}

} // namespace raft
} // namespace xzero
