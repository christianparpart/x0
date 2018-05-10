// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/Diagnostics.h>
#include <fmt/format.h>

namespace xzero::flow::diagnostics {

std::string Message::string() const {
  switch (type) {
    case Type::Warning:
      return fmt::format("[{}] {}", sourceLocation, text);
    case Type::LinkError:
      return fmt::format("{}: {}", type, text);
    default:
      return fmt::format("[{}] {}: {}", sourceLocation, type, text);
  }
}

bool Message::operator==(const Message& other) const noexcept {
  // XXX ignore SourceLocation's filename & end
  return type == other.type &&
         sourceLocation.begin == other.sourceLocation.begin &&
         text == other.text;
}

void Report::clear() {
  messages_.clear();
}

void Report::log() const {
  for (const Message& message: messages_) {
    switch (message.type) {
      case Type::Warning:
        fmt::print("Warning: {}\n", message);
        break;
      default:
        fmt::print("Error: {}\n", message);
        break;
    }
  }
}

bool Report::operator==(const Report& other) const noexcept {
  if (size() != other.size())
    return false;

  for (size_t i = 0, e = size(); i != e; ++i)
    if (messages_[i] != other.messages_[i])
      return false;

  return true;
}

bool Report::contains(const Message& message) const noexcept {
  for (const Message& m: messages_)
    if (m == message)
      return true;

  return false;
}

DifferenceReport difference(const Report& first, const Report& second) {
  DifferenceReport diff;

  for (const Message& m: first)
    if (!second.contains(m))
      diff.first.push_back(m);

  for (const Message& m: second)
    if (!first.contains(m))
      diff.second.push_back(m);

  return diff;
}

} // namespace xzero::flow::diagnostics
