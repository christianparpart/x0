#pragma once
/* <FlowValue.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow2/FlowType.h>
#include <x0/IPAddress.h>
#include <x0/Defines.h>
#include <x0/Cidr.h>
#include <x0/Api.h>

#include <string>
#include <cstring>
#include <cassert>
#include <cstddef>

namespace x0 {

class RegExp;
class FlowArray;
class SocketSpec;

struct X0_PACKED X0_API FlowValue
{
	typedef bool (*Handler)(void *);

	uint32_t type_;
	int64_t number_;
	union {
		const char* string_;
		const RegExp* regexp_;
		const IPAddress* ipaddr_;
		const Cidr* cidr_;
		Handler handler_;
		FlowValue* array_;
	};

	FlowValue();
	FlowValue(const FlowValue&);
	explicit FlowValue(bool boolean);
	explicit FlowValue(long long integer);
	explicit FlowValue(const RegExp* re);
	explicit FlowValue(const IPAddress* ip);
	explicit FlowValue(const Cidr* cidr);
	explicit FlowValue(const char* cstring);
	explicit FlowValue(const char* buffer, size_t length);
	explicit FlowValue(Handler handler);
	~FlowValue();

	FlowValue& set(bool boolean);
	FlowValue& set(int integer);
	FlowValue& set(unsigned int integer) { return set(static_cast<long long>(integer)); }
	FlowValue& set(long integer);
	FlowValue& set(unsigned long integer) { return set(static_cast<long long>(integer)); }
	FlowValue& set(long long integer);
	FlowValue& set(const RegExp* re);
	FlowValue& set(const IPAddress* ip);
	FlowValue& set(const Cidr& cidr);
	FlowValue& set(const char* cstring);
	FlowValue& set(const char* buffer, size_t length);
	FlowValue& set(const FlowValue& v);
	FlowValue& set(const FlowArray* array);

	void clear();

	FlowType type() const { return static_cast<FlowType>(type_); }

	template<typename T> bool load(T& result) const;

	bool isVoid() const { return type() == FlowType::Void; }
	bool isBool() const { return type() == FlowType::Boolean; }
	bool isNumber() const { return type() == FlowType::Number; }
	bool isRegExp() const { return type() == FlowType::RegExp; }
	bool isIPAddress() const { return type() == FlowType::IPAddress; }
	bool isCidr() const { return type() == FlowType::Cidr; }
	bool isString() const { return type() == FlowType::String; }
	bool isBuffer() const { return type() == FlowType::Buffer; }
	bool isArray() const { return type() == FlowType::Array; }
	bool isHandler() const { return type() == FlowType::Handler; }

	bool toBoolean() const;
	long long toNumber() const;
	const RegExp& toRegExp() const;
	const IPAddress& toIPAddress() const;
	const Cidr& toCidr() const;
	const char* toString() const;
	const FlowArray& toArray() const;
	Handler toHandler() const;

	std::string asString() const;

	void dump() const;
	void dump(bool linefeed) const;

private:
	void setType(FlowType t) { type_ = static_cast<decltype(type_)>(t); }
};

struct FlowValueOffset {
	enum Name {
		Type        = 0, //offsetof(FlowValue, type_),
		Boolean     = 1, //offsetof(FlowValue, number_),
		Number      = 1, //offsetof(FlowValue, number_),
		String      = 2, //offsetof(FlowValue, string_),
		RegExp      = 2, //offsetof(FlowValue, regexp_),
		IPAddress   = 2, //offsetof(FlowValue, ipaddr_),
		BufferData  = 2, //offsetof(FlowValue, string_),
		BufferSize  = 2, //offsetof(FlowValue, number_),
		ArrayData   = 2, //offsetof(FlowValue, array_),
		ArraySize   = 2, //offsetof(FlowValue, number_),
		Handler     = 2, //offsetof(FlowValue, handler_),
	};

	Name value;
	FlowValueOffset() = default;
	FlowValueOffset(Name v) : value(v) {}
	FlowValueOffset(int v) : value((Name) v) {}

	operator size_t () const { return (uint64_t) value; }
};


struct X0_API FlowArray : protected FlowValue
{
	FlowArray(int argc, FlowValue* argv) : FlowValue() {
		type_ = static_cast<decltype(type_)>(FlowType::Array);
		number_ = argc;
		array_ = argv;
	}

	bool empty() const { return number_ == 0; }
	size_t size() const { return number_; }

	const FlowValue& at(size_t i) const { return array_[i]; }
	FlowValue& at(size_t i) { return const_cast<FlowValue*>(array_)[i]; }

	const FlowValue& operator[](size_t i) const { return array_[i]; }
	FlowValue& operator[](size_t i) { return const_cast<FlowValue*>(array_)[i]; }

	const FlowValue* begin() const { return array_; }
	const FlowValue* end() const { return array_ + size(); }

	template<typename T>
	inline bool load(size_t i, T& out) const { return i < size() ? at(i).load(out) : false; }

	FlowArray shift(size_t n = 1) const { return FlowArray(size() - n, &(const_cast<FlowArray*>(this)->at(n))); }
};

typedef FlowArray FlowParams;

// {{{ inlines
inline const char* FlowValue::toString() const
{
	return string_;
}

inline long long FlowValue::toNumber() const
{
	return number_;
}

inline bool FlowValue::toBoolean() const
{
	return number_ != 0;
}

inline FlowValue& FlowValue::set(bool boolean)
{
	setType(FlowType::Boolean);
	number_ = boolean;
	return *this;
}
// }}}

} // namespace x0
