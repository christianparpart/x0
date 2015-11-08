// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <deque>
#include <string>

namespace xzero {

class Buffer;
class BufferRef;

class JsonWriter {
 private:
  enum class Type { Value, Object, Array };

  struct StackFrame {
    StackFrame(Type t) : type(t), fieldCount(0) {}

    Type type;
    size_t fieldCount;
  };

  Buffer& output_;
  std::deque<StackFrame> stack_;

  void indent();
  void begin(Type t);

  bool isSimple() const {
    return !stack_.empty() && stack_.back().type == Type::Value;
  }

  bool isComplex() const {
    return !stack_.empty() && stack_.back().type != Type::Value;
  }

  bool isValue() const {
    return !stack_.empty() && stack_.back().type == Type::Value;
  }

  bool isArray() const {
    return !stack_.empty() && stack_.back().type == Type::Array;
  }

  bool isObject() const {
    return !stack_.empty() && stack_.back().type == Type::Object;
  }

  size_t fieldCount(size_t roff = 0) const {
    return stack_[stack_.size() - 1 - roff].fieldCount;
  }

  void incrementFieldCount() {
    ++stack_.back().fieldCount;
  }

  void push(Type t) {
    stack_.push_back(StackFrame(t));
  }

  void pop() {
    stack_.pop_back();
  }

  void dumpStack();

 public:
  explicit JsonWriter(Buffer& output);

  Buffer& buffer() const { return output_; }

  JsonWriter& name(const std::string& name);  // "$NAME":

  JsonWriter& beginObject(const std::string& name = std::string());
  JsonWriter& endObject();

  JsonWriter& beginArray(const std::string& name);
  JsonWriter& endArray();

  template <typename T>
  JsonWriter& operator()(const T& value) {
    *this << value;
    return *this;
  }

  template <typename T>
  JsonWriter& value(const T& _value) {
    *this << _value;
    return *this;
  }

  void preValue();
  void postValue();
};

JsonWriter& operator<<(xzero::JsonWriter& json, bool value);
JsonWriter& operator<<(xzero::JsonWriter& json, char value);
JsonWriter& operator<<(xzero::JsonWriter& json, int value);
JsonWriter& operator<<(xzero::JsonWriter& json, long value);
JsonWriter& operator<<(xzero::JsonWriter& json, long long value);
JsonWriter& operator<<(xzero::JsonWriter& json, unsigned int value);
JsonWriter& operator<<(xzero::JsonWriter& json, unsigned long value);
JsonWriter& operator<<(xzero::JsonWriter& json, unsigned long long value);
JsonWriter& operator<<(xzero::JsonWriter& json, float value);
JsonWriter& operator<<(xzero::JsonWriter& json, const std::string& value);
JsonWriter& operator<<(xzero::JsonWriter& json, const Buffer& value);
JsonWriter& operator<<(xzero::JsonWriter& json, const BufferRef& value);
JsonWriter& operator<<(xzero::JsonWriter& json, const char* value);

}  // namespace xzero
