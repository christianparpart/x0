#pragma once
/* <FlowValue.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Api.h>
#include <x0/Defines.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>

#include <string>
#include <cstring>
#include <cassert>

namespace x0 {

class RegExp;
class FlowArray;
class SocketSpec;


struct X0_PACKED X0_API FlowValue
{
	typedef bool (*Handler)(void *);

	enum Type {
		VOID     = 0,	// nothing
		BOOLEAN  = 1,   // a boolean value
		NUMBER   = 2,   // an integer value
		REGEXP   = 3,   // RegExp instance, a precompiled regular expression
		STRING   = 4,   // zero-terminated C-string
		BUFFER   = 5,   // a string buffer with its length stored in the number_ field
		ARRAY    = 6,   // FlowArray instance, an array of Flow values
		IP       = 7,   // IPAddress instance, (IPv4 or IPv6 address)
		CIDR     = 8,   // CIDR subnet
		HANDLER  = 9,   // function pointer to user-defined handler
	};

	uint32_t type_;
	union {
		uint64_t number;
		bool boolean;
		const char* string;
		const RegExp* regexp;
		const IPAddress* ipaddr;
		const Cidr* cidr;
		Handler handler;
		struct {
			uint32_t size;
			FlowValue* values;
		} array;
		struct bufref {
			uint32_t size;
			const char* data;
		} bufref;
	} data;

	FlowValue();
	FlowValue(bool boolean);
	FlowValue(long long integer);
	FlowValue(const RegExp* re);
	FlowValue(const IPAddress* ip);
	FlowValue(const Cidr* cidr);
	FlowValue(const char* cstring);
	FlowValue(const char* buffer, size_t length);
	FlowValue(const FlowValue&);
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

	Type type() const { return static_cast<Type>(type_); }

	template<typename T> bool load(T& result) const;

	bool isVoid() const { return type_ == VOID; }
	bool isBool() const { return type_ == BOOLEAN; }
	bool isNumber() const { return type_ == NUMBER; }
	bool isRegExp() const { return type_ == REGEXP; }
	bool isIPAddress() const { return type_ == IP; }
	bool isCidr() const { return type_ == CIDR; }
	bool isString() const { return type_ == STRING; }
	bool isBuffer() const { return type_ == BUFFER; }
	bool isArray() const { return type_ == ARRAY; }
	bool isHandler() const { return type_ == HANDLER; }

	bool toBool() const;
	long long toNumber() const;
	const RegExp& toRegExp() const;
	const IPAddress& toIPAddress() const;
	const char* toString() const;
	const FlowArray& toArray() const;
	Handler toHandler() const;

	std::string asString() const;

	void dump() const;
	void dump(bool linefeed) const;
};

class X0_API FlowArray : protected FlowValue {
public:
	FlowArray(int argc, FlowValue* argv) :
		FlowValue()
	{
		type_ = ARRAY;
		data.array.size = argc;
		data.array.values = argv;
	}

	bool empty() const { return data.array.size == 0; }
	size_t size() const { return data.array.size; }

	const FlowValue& at(size_t i) const { return data.array.values[i]; }
	FlowValue& at(size_t i) { return const_cast<FlowValue*>(data.array.values)[i]; }

	const FlowValue& operator[](size_t i) const { return data.array.values[i]; }
	FlowValue& operator[](size_t i) { return const_cast<FlowValue*>(data.array.values)[i]; }

	const FlowValue* begin() const { return data.array.values; }
	const FlowValue* end() const { return data.array.values + size(); }

	template<typename T>
	inline bool load(size_t i, T& out) const { return i < size() ? at(i).load(out) : false; }

	FlowArray shift(size_t n = 1) const { return FlowArray(size() - n, &(const_cast<FlowArray*>(this)->at(n))); }
};

typedef FlowArray FlowParams;

// {{{ inlines
// }}}

} // namespace x0
