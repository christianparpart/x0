/* <lib/base/JsonWriter.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
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
	for (int i = 0, e = 2 * stack_.size(); i != e; ++i) {
		output_.push_back(' ');
	}
}

void JsonWriter::begin(Type t)
{
	if (!stack_.empty()) {
		if (fieldCount() > 0) {
			output_ << ",\n";
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
	begin(Type::Object);

	if (!name.empty())
		output_ << "\"" << name << "\": {\n";
	else
		output_ << "{\n";

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

JsonWriter& JsonWriter::operator()(char value)
{
	preValue();
	output_ << "\"" << value << "\"";
	postValue();

	return *this;
}

JsonWriter& JsonWriter::operator()(const Buffer& value)
{
	preValue();
	output_ << "\"" << value << "\"";
	postValue();

	return *this;
}

JsonWriter& JsonWriter::operator()(const BufferRef& value)
{
	preValue();
	output_ << "\"" << value << "\"";
	postValue();

	return *this;
}

JsonWriter& JsonWriter::operator()(const std::string& value)
{
	preValue();
	output_ << "\"" << value << "\"";
	postValue();

	return *this;
}

} // namespace x0
