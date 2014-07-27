// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <base/Api.h>
#include <deque>
#include <string>

namespace base {

class Buffer;
class BufferRef;

class BASE_API JsonWriter {
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
  void incrementFieldCount() { ++stack_.back().fieldCount; }

  void push(Type t) { stack_.push_back(StackFrame(t)); }
  void pop() { stack_.pop_back(); }

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

BASE_API JsonWriter& operator<<(base::JsonWriter& json, bool value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, char value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, int value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, long value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, long long value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, unsigned int value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, unsigned long value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, unsigned long long value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, float value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, const std::string& value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, const Buffer& value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, const BufferRef& value);
BASE_API JsonWriter& operator<<(base::JsonWriter& json, const char* value);

}  // namespace base