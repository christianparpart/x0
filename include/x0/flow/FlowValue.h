/* <flow/FlowValue.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_flow_value_h
#define x0_flow_value_h

#include <x0/Api.h>
#include <x0/IPAddress.h>

#include <string>
#include <cstring>
#include <cassert>

namespace x0 {

class RegExp;
class FlowArray;

struct __attribute__((packed)) X0_API FlowValue
{
	typedef bool (*Function)(void *);

	enum Type {
		VOID     = 0, // nothing
		BOOLEAN  = 1, // a boolean value
		NUMBER   = 2, // an integer value
		REGEXP   = 3, // RegExp instance, a precompiled regular expression
		STRING   = 4, // zero-terminated C-string
		BUFFER   = 5, // a string buffer with its length stored in the number_ field
		ARRAY    = 6, // FlowArray instance, an array of Flow values
		IP       = 7, // IPAddress instance, (IPv4 or IPv6 address)
		FUNCTION = 8  // function pointer to a user-specified handler
	};

	static const int TypeOffset = 0;
	static const int NumberOffset = 1;
	static const int RegExpOffset = 2;
	static const int IPAddrOffset = 2;
	static const int BufferOffset = 2;
	static const int ArrayOffset = 2;
	static const int FunctionOffset = 2;

	uint32_t type_;
	unsigned long long number_;
	union {
		const char* string_;
		const FlowValue* array_;
		const RegExp* regexp_;
		const IPAddress* ipaddress_;
		Function function_;
	};

	FlowValue();
	FlowValue(bool boolean);
	FlowValue(unsigned long long integer);
	FlowValue(const RegExp* re);
	FlowValue(const IPAddress* ip);
	FlowValue(const char* cstring);
	FlowValue(const char* buffer, size_t length);
	FlowValue(const FlowValue&);
	explicit FlowValue(Function function);
	~FlowValue();

	void clear();

	FlowValue& set(bool boolean);
	FlowValue& set(int integer);
	FlowValue& set(unsigned long integer);
	FlowValue& set(unsigned long long integer);
	FlowValue& set(const RegExp* re);
	FlowValue& set(const IPAddress* ip);
	FlowValue& set(const char* cstring);
	FlowValue& set(const char* buffer, size_t length);
	FlowValue& set(const FlowValue& v);
	FlowValue& set(const FlowArray* array);
	FlowValue& set(Function function);

	FlowValue& operator=(const FlowValue&);
	FlowValue& operator=(bool value);
	FlowValue& operator=(const RegExp* value);
	FlowValue& operator=(const IPAddress* value);
	FlowValue& operator=(const char* value);
	FlowValue& operator=(unsigned long long value);
	FlowValue& operator=(unsigned int value);
	FlowValue& operator=(Function function);

	Type type() const { return static_cast<Type>(type_); }

	template<typename T> bool load(T& result) const;

	bool isVoid() const { return type_ == VOID; }
	bool isBool() const { return type_ == BOOLEAN; }
	bool isNumber() const { return type_ == NUMBER; }
	bool isRegExp() const { return type_ == REGEXP; }
	bool isIPAddress() const { return type_ == IP; }
	bool isString() const { return type_ == STRING; }
	bool isArray() const { return type_ == ARRAY; }
	bool isFunction() const { return type_ == FUNCTION; }

	bool toBool() const;
	unsigned long long toNumber() const;
	const RegExp* toRegExp() const;
	const IPAddress* toIPAddress() const;
	const char* toString() const;
	const FlowArray& toArray() const;
	Function toFunction() const;

	void dump() const;
	void dump(bool linefeed) const;
};

class FlowArray : public FlowValue {
public:
	bool empty() const { return number_ == 0; }
	size_t size() const { return number_; }

	const FlowValue& at(size_t i) const { return array_[i]; }
	FlowValue& at(size_t i) { return const_cast<FlowValue*>(array_)[i]; }

	const FlowValue& operator[](size_t i) const { return array_[i]; }
	FlowValue& operator[](size_t i) { return const_cast<FlowValue*>(array_)[i]; }

	const FlowValue* begin() const { return array_; }
	const FlowValue* end() const { return array_ + size(); }
};

// {{{ inlines
inline FlowValue::FlowValue() :
	type_(VOID)
{
}

inline FlowValue::FlowValue(bool value) :
	type_(BOOLEAN),
	number_(value ? 1 : 0)
{
}

inline FlowValue::FlowValue(unsigned long long value) :
	type_(NUMBER),
	number_(value)
{
}

inline FlowValue::FlowValue(const RegExp* value) :
	type_(REGEXP),
	regexp_(value)
{
}

inline FlowValue::FlowValue(const IPAddress* value) :
	type_(IP),
	ipaddress_(value)
{
}

inline FlowValue::FlowValue(const char* value) :
	type_(STRING),
	number_(strlen(value)),
	string_(value)
{
}

inline FlowValue::FlowValue(Function value) :
	type_(FUNCTION),
	number_(),
	function_(value)
{
}

inline FlowValue::FlowValue(const FlowValue& v)
{
	*this = v;
}

inline FlowValue::~FlowValue()
{
}

inline void FlowValue::clear()
{
	type_ = VOID;
}

inline FlowValue& FlowValue::set(bool value)
{
	type_ = BOOLEAN;
	number_ = value ? 1 : 0;

	return *this;
}

inline FlowValue& FlowValue::set(int value)
{
	type_ = NUMBER;
	number_ = value;

	return *this;
}

inline FlowValue& FlowValue::set(unsigned long value)
{
	type_ = NUMBER;
	number_ = value;

	return *this;
}

inline FlowValue& FlowValue::set(unsigned long long value)
{
	type_ = NUMBER;
	number_ = value;

	return *this;
}

inline FlowValue& FlowValue::set(const RegExp* re)
{
	type_ = REGEXP;
	regexp_ = re;

	return *this;
}

inline FlowValue& FlowValue::set(const IPAddress* value)
{
	type_ = IP;
	ipaddress_ = value;

	return *this;
}

inline FlowValue& FlowValue::set(const char* cstring)
{
	type_ = STRING;
	if (cstring)
	{
		number_ = strlen(cstring);
		string_ = cstring;
	}
	else
	{
		number_ = 0;
		string_ = "";
	}

	return *this;
}

inline FlowValue& FlowValue::set(const char* buffer, size_t length)
{
	type_ = BUFFER;
	number_ = length;
	string_ = buffer ? buffer : "";

	return *this;
}

inline FlowValue& FlowValue::set(const FlowValue& value)
{
	type_ = value.type_;
	number_ = value.number_;
	string_ = value.string_;

	return *this;
}

inline FlowValue& FlowValue::set(const FlowArray* array)
{
	type_ = ARRAY;
	number_ = 0;
	array_ = array;
	return *this;
}

inline FlowValue& FlowValue::set(Function value)
{
	type_ = FUNCTION;
	number_ = 0;
	function_ = value;
	return *this;
}

inline FlowValue& FlowValue::operator=(const FlowValue& value)
{
	return set(value);
}

inline FlowValue& FlowValue::operator=(bool value)
{
	return set(value);
}

inline FlowValue& FlowValue::operator=(const RegExp* value)
{
	return set(value);
}

inline FlowValue& FlowValue::operator=(const IPAddress* value)
{
	return set(value);
}

inline FlowValue& FlowValue::operator=(const char* value)
{
	return set(value);
}

inline FlowValue& FlowValue::operator=(unsigned long long value)
{
	return set(value);
}

inline FlowValue& FlowValue::operator=(unsigned int value)
{
	return set((unsigned long long)value);
}

inline FlowValue& FlowValue::operator=(Function value)
{
	return set(value);
}

inline bool FlowValue::toBool() const
{
	return number_ != 0;
}

inline unsigned long long FlowValue::toNumber() const
{
	return number_;
}

inline const RegExp* FlowValue::toRegExp() const
{
	return regexp_;
}

inline const IPAddress* FlowValue::toIPAddress() const
{
	return ipaddress_;
}

inline const char* FlowValue::toString() const
{
	return string_;
}

inline const FlowArray& FlowValue::toArray() const
{
	return *static_cast<const FlowArray*>(this);
}

inline FlowValue::Function FlowValue::toFunction() const
{
	assert(isFunction());
	return function_;
}

template<>
inline bool FlowValue::load<bool>(bool& result) const
{
	if (!isBool())
		return false;

	result = toBool();
	return true;
}

template<>
inline bool FlowValue::load<int>(int& result) const
{
	if (!isNumber())
		return false;

	result = toNumber();
	return true;
}

template<>
inline bool FlowValue::load<long>(long& result) const
{
	if (!isNumber())
		return false;

	result = toNumber();
	return true;
}

template<>
inline bool FlowValue::load<unsigned long long>(unsigned long long& result) const
{
	if (!isNumber())
		return false;

	result = toNumber();
	return true;
}

template<>
inline bool FlowValue::load<std::string>(std::string& result) const
{
	if (isString())
	{
		result = toString();
		return true;
	}

	return false;
}

template<>
inline bool FlowValue::load<IPAddress>(IPAddress& result) const
{
	if (!isIPAddress())
		return false;

	result = *toIPAddress();
	return true;
}

template<>
inline bool FlowValue::load<FlowValue::Function>(Function& result) const
{
	if (!isFunction())
		return false;

	result = toFunction();
	return true;
}
// }}}

} // namespace x0

#endif
