/* <x0/JsonWriter.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include <x0/Api.h>
#include <deque>
#include <string>

namespace x0 {

class Buffer;
class BufferRef;

class X0_API JsonWriter
{
private:
	enum class Type {
		Value, Object, Array
	};

	struct StackFrame {
		StackFrame(Type t) : type(t), fieldCount(0) {}

		Type type;
		size_t fieldCount;
	};

	Buffer& output_;
	std::deque<StackFrame> stack_;

	void indent();
	void begin(Type t);
	void preValue();
	void postValue();

	bool isSimple() const { return stack_.back().type == Type::Value; }
	bool isComplex() const { return stack_.back().type != Type::Value; }

	bool isValue() const { return stack_.back().type == Type::Value; }
	bool isArray() const { return stack_.back().type == Type::Array; }
	bool isObject() const { return stack_.back().type == Type::Object; }

	size_t fieldCount(size_t roff = 0) const { return stack_[stack_.size() - 1 - roff].fieldCount; }
	void incrementFieldCount() { ++stack_.back().fieldCount; }

	void push(Type t) { stack_.push_back(StackFrame(t)); }
	void pop() { stack_.pop_back(); }

	void dumpStack();

public:
	explicit JsonWriter(Buffer& output);

	JsonWriter& name(const std::string& name); // "$NAME":

	JsonWriter& beginObject(const std::string& name = std::string());
	JsonWriter& endObject();

	JsonWriter& beginArray(const std::string& name);
	JsonWriter& endArray();

	template<typename T>
	JsonWriter& operator()(const T& value)
	{
		preValue();
		output_ << value;
		postValue();

		return *this;
	}

	JsonWriter& operator()(char value);
	JsonWriter& operator()(const Buffer& value);
	JsonWriter& operator()(const BufferRef& value);
	JsonWriter& operator()(const std::string& value);
};

} // namespace x0
