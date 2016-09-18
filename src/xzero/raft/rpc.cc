// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/raft/rpc.h>

namespace xzero {
namespace raft {

// {{{ LogEntry
LogEntry::LogEntry()
    : LogEntry(0, 0) {
}

LogEntry::LogEntry(Term term, Index index)
    : LogEntry(term, index, Command()) {
}

LogEntry::LogEntry(Term term,
                   Index index,
                   Command&& cmd)
    : LogEntry(term, index, LOG_COMMAND, std::move(cmd)) {
}

LogEntry::LogEntry(Term term,
                   Index index,
                   LogType type)
    : LogEntry(term, index, type, Command()) {
}


LogEntry::LogEntry(Term term,
                   Index index,
                   LogType type,
                   Command&& cmd)
    : term_(term),
      index_(index),
      type_(type),
      command_(cmd) {
}
// }}}

} // namespace raft
} // namespace xzero
