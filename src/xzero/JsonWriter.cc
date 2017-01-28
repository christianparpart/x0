// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/JsonWriter.h>
#include <xzero/Buffer.h>
#include <atomic>

namespace xzero {

JsonWriter::JsonWriter(Buffer* output)
    : output_(*output),
      stack_() {
}

void JsonWriter::indent() {
  for (size_t i = 0, e = stack_.size(); i != e; ++i) {
    output_.push_back("  ");
  }
}

void JsonWriter::begin(Type t) {
  if (!stack_.empty()) {
    if (fieldCount() > 0) {
      output_ << ",\n";
    } else if (isArray() && fieldCount() == 0) {
      output_ << "\n";
    }

    indent();

    incrementFieldCount();
  }

  push(t);
}

void JsonWriter::preValue() {
  if (isComplex()) {
    if (fieldCount() > 0) {  // i.e. Array
      output_ << ",\n";
    } else {
      output_ << "\n";
    }
    indent();
  }
  incrementFieldCount();
}

void JsonWriter::postValue() {
  if (isValue()) {
    pop();
  }
}

JsonWriter& JsonWriter::name(const std::string& name)  // "$NAME":
{
  begin(Type::Value);

  output_ << "\"" << name << "\": ";

  return *this;
}

JsonWriter& JsonWriter::beginObject(const std::string& name)  // { ... }
{
  if (!name.empty()) {
    begin(Type::Object);
    output_ << "\"" << name << "\": {\n";
  } else {
    if (isValue()) {
      stack_.back().type = Type::Object;
    } else {
      begin(Type::Object);
    }
    output_ << "{\n";
  }

  return *this;
}

JsonWriter& JsonWriter::endObject() {
  output_ << "\n";
  pop();
  indent();
  output_ << "}";

  return *this;
}

JsonWriter& JsonWriter::beginArray(const std::string& name)  // [ ... ]
{
  begin(Type::Array);
  output_ << "\"" << name << "\": [";

  return *this;
}

JsonWriter& JsonWriter::endArray() {
  output_ << "\n";
  pop();
  indent();
  output_ << "]";

  return *this;
}

template<>
JsonWriter& JsonWriter::value(const bool& value) {
  preValue();
  buffer() << (value ? "true" : "false");
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const char& value) {
  preValue();
  buffer() << '"' << value << '"';
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const int& value) {
  preValue();
  buffer() << value;
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const long& value) {
  preValue();
  buffer() << value;
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const long long& value) {
  preValue();
  buffer() << value;
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const unsigned int& value) {
  preValue();
  buffer() << value;
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const unsigned long& value) {
  preValue();
  buffer() << value;
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const unsigned long long& value) {
  preValue();
  buffer() << value;
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const float& value) {
  preValue();

  char buf[128];
  ssize_t n = snprintf(buf, sizeof(buf), "%f", value);
  buffer().push_back(buf, n);

  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const std::string& value) {
  preValue();
  buffer() << '"' << value << '"';
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const Buffer& value) {
  preValue();
  buffer() << '"' << value << '"';
  postValue();
  return *this;
}

template<>
JsonWriter& JsonWriter::value(const BufferRef& value) {
  preValue();
  buffer() << '"' << value << '"';
  postValue();
  return *this;
}

// template<>
// JsonWriter& JsonWriter::value(const char* value) {
//   buffer() << '"' << value << '"';
//   postValue();
//   return *this;
// }

template<>
JsonWriter& JsonWriter::value(const std::atomic<unsigned long long>& value) {
  preValue();
  buffer() << value.load();
  postValue();
  return *this;
}

}  // namespace xzero
