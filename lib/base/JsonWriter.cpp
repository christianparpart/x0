/* <lib/base/JsonWriter.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/JsonWriter.h>
#include <x0/Buffer.h>

namespace x0 {

JsonWriter::JsonWriter(Buffer& output) :
    output_(output),
    stack_()
{
}

void JsonWriter::indent()
{
    for (size_t i = 0, e = stack_.size(); i != e; ++i) {
        output_.push_back("  ");
    }
}

void JsonWriter::begin(Type t)
{
    if (!stack_.empty()) {
        if (fieldCount() > 0) {
            output_ << ",\n";
        }
        else if (isArray() && fieldCount() == 0) {
            output_ << "\n";
        }

        indent();

        incrementFieldCount();
    }

    push(t);
}

void JsonWriter::preValue()
{
    if (isComplex()) {
        if (fieldCount() > 0) { // i.e. Array
            output_ << ",\n";
        } else {
            output_ << "\n";
        }
        indent();
    }
    incrementFieldCount();
}

void JsonWriter::postValue()
{
    if (isValue()) {
        pop();
    }
}

JsonWriter& JsonWriter::name(const std::string& name) // "$NAME":
{
    begin(Type::Value);

    output_ << "\"" << name << "\": ";

    return *this;
}

JsonWriter& JsonWriter::beginObject(const std::string& name) // { ... }
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

JsonWriter& JsonWriter::endObject()
{
    output_ << "\n";
    pop();
    indent();
    output_ << "}";

    return *this;
}

JsonWriter& JsonWriter::beginArray(const std::string& name) // [ ... ]
{
    begin(Type::Array);
    output_ << "\"" << name << "\": [";

    return *this;
}

JsonWriter& JsonWriter::endArray()
{
    output_ << "\n";
    pop();
    indent();
    output_ << "]";

    return *this;
}

JsonWriter& operator<<(JsonWriter& json, bool value)
{
    json.preValue();
    json.buffer() << (value ? "true" : "false");
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, char value)
{
    json.preValue();
    json.buffer() << '"' << value << '"';
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, int value)
{
    json.preValue();
    json.buffer() << value;
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, long value)
{
    json.preValue();
    json.buffer() << value;
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, long long value)
{
    json.preValue();
    json.buffer() << value;
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, unsigned int value)
{
    json.preValue();
    json.buffer() << value;
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, unsigned long value)
{
    json.preValue();
    json.buffer() << value;
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, unsigned long long value)
{
    json.preValue();
    json.buffer() << value;
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, float value)
{
    json.preValue();

    char buf[128];
    ssize_t n = snprintf(buf, sizeof(buf), "%f", value);
    json.buffer().push_back(buf, n);

    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, const std::string& value)
{
    json.preValue();
    json.buffer() << '"' << value << '"';
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, const Buffer& value)
{
    json.preValue();
    json.buffer() << '"' << value << '"';
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, const BufferRef& value)
{
    json.preValue();
    json.buffer() << '"' << value << '"';
    json.postValue();
    return json;
}

JsonWriter& operator<<(JsonWriter& json, const char* value)
{
    json.preValue();
    json.buffer() << '"' << value << '"';
    json.postValue();
    return json;
}

} // namespace x0
