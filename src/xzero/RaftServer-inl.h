namespace xzero {

// {{{ StaticDiscovery
inline RaftServer::StaticDiscovery::StaticDiscovery(
    std::initializer_list<Id>&& list)
    : members_(list) {
}
// }}} StaticDiscovery
// {{{ LogEntry
inline RaftServer::LogEntry::LogEntry()
    : LogEntry(0, 0) {
}

inline RaftServer::LogEntry::LogEntry(Term term, Index index)
    : LogEntry(term, index, Command()) {
}

inline RaftServer::LogEntry::LogEntry(Term term,
                                      Index index,
                                      Command&& cmd)
    : LogEntry(term, index, LOG_COMMAND, std::move(cmd)) {
}

inline RaftServer::LogEntry::LogEntry(Term term,
                                      Index index,
                                      LogType type)
    : LogEntry(term, index, type, Command()) {
}


inline RaftServer::LogEntry::LogEntry(Term term,
                                      Index index,
                                      LogType type,
                                      Command&& cmd)
    : term_(term),
      index_(index),
      type_(type),
      command_(cmd) {
}
// }}}

} // namespace xzero
