/* <x0/BufferRef.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_BufferRef_h
#define sw_x0_BufferRef_h (1)

#include <x0/strutils.h>

#include <cassert>
#include <cstring>
#include <cstddef>
#include <climits>

#include <cstdlib>
#include <string>
#include <stdexcept>

namespace x0 {

//! \addtogroup base
//@{

/** holds a reference to a region of a buffer
 */
class BufferRef
{
public:
	typedef char value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;

	static const std::size_t npos = std::size_t(-1);

protected:
	char* data_;
	std::size_t size_;

public:
	BufferRef();
	BufferRef(const char* buffer, std::size_t _size);
	BufferRef(const unsigned char* buffer, std::size_t _size);
//	BufferRef(iterator begin, iterator end);
//	BufferRef(const char* buffer, std::size_t n);
//	BufferRef(const char* cstring);
//	BufferRef(const std::string& string);
//	template<typename PodType, std::size_t N> explicit BufferRef(PodType (&value)[N]);

	BufferRef(const BufferRef& v);

	BufferRef& operator=(const BufferRef& v);

	void clear();

	// properties
	bool empty() const;
	std::size_t size() const;

	const value_type* data() const;

	operator bool() const;
	bool operator!() const;

	// iterator access
	iterator begin() const;
	iterator end() const;

	const_iterator cbegin() const;
	const_iterator cend() const;

	void shl(ssize_t offset = 1);
	void shr(ssize_t offset = 1);

	// find
	template<typename PodType, std::size_t N>
	std::size_t find(PodType (&value)[N], std::size_t offset = 0) const;
	std::size_t find(const value_type *value, std::size_t offset = 0) const;
	std::size_t find(const BufferRef& value, std::size_t offset = 0) const;
	std::size_t find(value_type value, std::size_t offset = 0) const;

	// rfind
	template<typename PodType, std::size_t N> std::size_t rfind(PodType (&value)[N]) const;
	std::size_t rfind(const value_type *value) const;
	std::size_t rfind(const BufferRef& value) const;
	std::size_t rfind(value_type value) const;
	std::size_t rfind(value_type value, std::size_t offset) const;

	// begins / ibegins
	bool begins(const BufferRef& value) const;
	bool begins(const value_type* value) const;
	bool begins(const std::string& value) const;
	bool begins(value_type value) const;

	bool ibegins(const BufferRef& value) const;
	bool ibegins(const value_type* value) const;
	bool ibegins(const std::string& value) const;
	bool ibegins(value_type value) const;

	// ends / iends
	bool ends(const BufferRef& value) const;
	bool ends(const value_type* value) const;
	bool ends(const std::string& value) const;
	bool ends(value_type value) const;

	bool iends(const BufferRef& value) const;
	bool iends(const value_type* value) const;
	bool iends(const std::string& value) const;
	bool iends(value_type value) const;

	// sub
	BufferRef ref(std::size_t offset = 0) const;
	BufferRef ref(std::size_t offset, std::size_t size) const;

	BufferRef operator()(std::size_t offset = 0) const;
	BufferRef operator()(std::size_t offset, std::size_t count) const;

	// random access
	const value_type& operator[](std::size_t offset) const;

	// mutation
	BufferRef chomp() const;

	// STL string
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;

	// casts
	template<typename T> T as() const;
	template<typename T> T hex() const;

	bool toBool() const;
	int toInt() const;
	double toDouble() const;
	float toFloat() const;
};

// free functions
bool equals(const BufferRef& a, const BufferRef& b);
bool equals(const BufferRef& a, const std::string& b);
template<typename PodType, std::size_t N> bool equals(const BufferRef& a, PodType (&b)[N]);

bool iequals(const BufferRef& a, const BufferRef& b);
bool iequals(const BufferRef& a, const std::string& b);
template<typename PodType, std::size_t N> bool iequals(const BufferRef& a, PodType (&b)[N]);

bool operator==(const BufferRef& a, const BufferRef& b);
bool operator==(const BufferRef& a, const char* b);
template<typename PodType, std::size_t N> bool operator==(const BufferRef& a, PodType (&b)[N]);

// {{{ BufferRef impl
inline BufferRef::BufferRef() :
	data_(nullptr),
	size_(0)
{
}

inline BufferRef::BufferRef(const char* buffer, std::size_t size) :
	data_(const_cast<char*>(buffer)),
	size_(size)
{
}

inline BufferRef::BufferRef(const unsigned char* buffer, std::size_t size) :
	data_((char*) buffer),
	size_(size)
{
}

#if 0
inline BufferRef::BufferRef(iterator begin, iterator end) :
	begin_(begin),
	end_(end)
{
	assert(begin_ <= end_);
}

inline BufferRef::BufferRef(const char* buffer, std::size_t n) :
	begin_(const_cast<char*>(buffer)),
	end_(const_cast<char*>(buffer) + n)
{
}

inline BufferRef::BufferRef(const char* cstring) :
	begin_(const_cast<char*>(cstring)),
	end_(const_cast<char*>(cstring) + strlen(cstring))
{
}

inline BufferRef::BufferRef(const std::string& value) :
	begin_(const_cast<char*>(value.data())),
	end_(begin_ + value.size())
{
}

template<typename PodType, std::size_t N>
inline BufferRef::BufferRef(PodType (&value)[N]) :
	begin_(const_cast<char*>(value)),
	end_(const_cast<char*>(value) + (N - 1))
{
}
#endif

inline BufferRef::BufferRef(const BufferRef& v) :
	data_(v.data_),
	size_(v.size_)
{
}

inline BufferRef& BufferRef::operator=(const BufferRef& v)
{
	data_ = v.data_;
	size_ = v.size_;

	return *this;
}

inline void BufferRef::clear()
{
	size_ = 0;
}

inline bool BufferRef::empty() const
{
	return size_ == 0;
}

inline std::size_t BufferRef::size() const
{
	return size_;
}

inline const BufferRef::value_type *BufferRef::data() const
{
	return data_;
}

inline BufferRef::operator bool() const
{
	return !empty();
}

inline bool BufferRef::operator!() const
{
	return empty();
}

inline BufferRef::iterator BufferRef::begin() const
{
	return const_cast<value_type*>(data());
}

inline BufferRef::iterator BufferRef::end() const
{
	return begin() + size();
}

inline BufferRef::const_iterator BufferRef::cbegin() const
{
	return data();
}

inline BufferRef::const_iterator BufferRef::cend() const
{
	return cbegin() + size();
}

/** shifts view's left margin by given bytes to the left, thus, increasing view's size.
 */
inline void BufferRef::shl(ssize_t value)
{
	data_ -= value;
}

/** shifts view's right margin by given bytes to the right, thus, increasing view's size.
 */
inline void BufferRef::shr(ssize_t value)
{
	size_ += value;
}

inline std::size_t BufferRef::find(const value_type *value, std::size_t offset) const
{
	const char *i = begin() + offset;
	const char *e = end();
	const int value_length = strlen(value);

	while (i != e)
	{
		if (*i == *value)
		{
			const char *p = i + 1;
			const char *q = value + 1; 
			const char *qe = i + value_length;

			while (*p == *q && p != qe)
			{
				++p;
				++q;
			}

			if (p == qe)
				return i - data();
		}
		++i;
	}

	return npos;
}

inline std::size_t BufferRef::find(value_type value, std::size_t offset) const
{
	if (const char *p = (const char *)memchr((const void *)(data() + offset), value, size() - offset))
		return p - data();

	return npos;
}

template<typename PodType, std::size_t N>
inline std::size_t BufferRef::find(PodType (&value)[N], std::size_t offset) const
{
	const char *i = data() + offset;
	const char *e = i + size() - offset;

	while (i != e)
	{
		if (*i == *value)
		{
			const char *p = i + 1;
			const char *q = value + 1; 
			const char *qe = i + N - 1;

			while (*p == *q && p != qe)
			{
				++p;
				++q;
			}

			if (p == qe)
				return i - data();
		}
		++i;
	}

	return npos;
}

inline std::size_t BufferRef::rfind(const value_type *value) const
{
	assert(0 && "not implemented");
	return npos;
}

inline std::size_t BufferRef::rfind(value_type value) const
{
	return rfind(value, size() - 1);
}

inline std::size_t BufferRef::rfind(value_type value, std::size_t offset) const
{
	if (empty())
		return npos;

	const char *p = data();
	const char *q = p + offset;

	for (;;)
	{
		if (*q == value)
			return q - p;

		if (p == q)
			break;

		--q;
	}

	return npos;
}

template<typename PodType, std::size_t N>
std::size_t BufferRef::rfind(PodType (&value)[N]) const
{
	if (empty())
		return npos;

	if (size() < N - 1)
		return npos;

	const char *i = end();
	const char *e = begin() + (N - 1);

	while (i != e)
	{
		if (*i == value[N - 1])
		{
			bool found = true;

			for (int n = 0; n < N - 2; ++n)
			{
				if (i[n] != value[n])
				{
					found = !found;
					break;
				}
			}

			if (found)
			{
				return i - begin();
			}
		}
		--i;
	}

	return npos;
}

inline bool BufferRef::begins(const BufferRef& value) const
{
	return value.size() <= size() && memcmp(data(), value.data(), value.size()) == 0;
}

inline bool BufferRef::begins(const value_type *value) const
{
	if (!value)
		return true;

	size_t len = std::strlen(value);
	return len <= size() && memcmp(data(), value, len) == 0;
}

inline bool BufferRef::begins(const std::string& value) const
{
	if (value.empty())
		return true;

	return value.size() <= size() && memcmp(data(), value.data(), value.size()) == 0;
}

inline bool BufferRef::begins(value_type value) const
{
	return size() >= 1 && data()[0] == value;
}

inline bool BufferRef::ends(value_type value) const
{
	return size() >= 1 && data()[size() - 1] == value;
}

inline bool BufferRef::ends(const value_type *value) const
{
	if (!value)
		return true;

	size_t valueLength = std::strlen(value);

	if (size() < valueLength)
		return false;

	return memcmp(data() + size() - valueLength, value, valueLength) == 0;
}

inline BufferRef BufferRef::ref(std::size_t offset) const
{
	assert(offset <= size_);
	return BufferRef(data_ + offset, size_ - offset);
}

inline BufferRef BufferRef::ref(std::size_t offset, std::size_t size) const
{
	assert(offset <= size_);
	assert(size == npos || offset + size <= size_);

	return size != npos
		? BufferRef(data_ + offset, size)
		: BufferRef(data_ + offset, size_ - offset);
}

inline BufferRef BufferRef::operator()(std::size_t offset) const
{
	return ref(offset);
}

inline BufferRef BufferRef::operator()(std::size_t offset, std::size_t size) const
{
	return ref(offset, size);
}

inline const BufferRef::value_type& BufferRef::operator[](std::size_t offset) const
{
	return data()[offset];
}

inline BufferRef BufferRef::chomp() const
{
	return ends('\n')
		? ref(0, size_ - 1)
		: ref(0, size_);
}

inline std::string BufferRef::str() const
{
	return substr(0);
}

inline std::string BufferRef::substr(std::size_t offset) const
{
	assert(offset <= size());
	ssize_t count = size() - offset;
	return count
		? std::string(data() + offset, count)
		: std::string();
}

inline std::string BufferRef::substr(std::size_t offset, std::size_t count) const
{
	assert(offset + count <= size());
	return count
		? std::string(data() + offset, count)
		: std::string();
}

template<>
inline bool BufferRef::as<bool>() const
{
	if (iequals(*this, "true"))
		return true;

	if (equals(*this, "1"))
		return true;

	return false;
}

template<>
inline int BufferRef::as<int>() const
{
	auto i = begin();
	auto e = end();

	// empty string
	if (i == e)
		return 0;

	// parse sign
	bool sign = false;
	if (*i == '-')
	{
		sign = true;
		++i;

		if (i == e)
			return 0;
	}
	else if (*i == '+')
	{
		++i;

		if (i == e)
			return 0;
	}

	// parse digits
	int val = 0;
	while (i != e)
	{
		if (*i < '0' || *i > '9')
			break;

		if (val)
			val *= 10;

		val += *i++ - '0';
	}

	// parsing succeed.
	if (sign)
		val = -val;

	return val;
}

template<>
inline double BufferRef::as<double>() const
{
	char* endptr = nullptr;
	double result = strtod(data(), &endptr);
	if (endptr <= end())
		return result;

	return 0.0;
}

template<>
inline float BufferRef::as<float>() const
{
	char* tmp = (char*) alloca(size() + 1);
	std::memcpy(tmp, data(), size());
	tmp[size()] = '\0';

	return strtof(tmp, nullptr);
}

template<typename T>
inline T BufferRef::hex() const
{
	auto i = begin();
	auto e = end();

	// empty string
	if (i == e)
		return 0;

	T val = 0;
	while (i != e)
	{
		if (!std::isxdigit(*i))
			break;

		if (val)
			val *= 16;

		if (std::isdigit(*i))
			val += *i++ - '0';
		else if (*i >= 'a' && *i <= 'f')
			val += 10 + *i++ - 'a';
		else // 'A' .. 'F'
			val += 10 + *i++ - 'A';
	}

	return val;
}

inline bool BufferRef::toBool() const
{
	return as<bool>();
}

inline int BufferRef::toInt() const
{
	return as<int>();
}

inline double BufferRef::toDouble() const
{
	return as<double>();
}

inline float BufferRef::toFloat() const
{
	return as<float>();
}
// }}}

// {{{ free function impl
inline bool equals(const BufferRef& a, const BufferRef& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

template<typename PodType, std::size_t N>
bool equals(const BufferRef& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	return std::memcmp(a.data(), b, bsize) == 0;
}

inline bool equals(const BufferRef& a, const std::string& b)
{
	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), b.size()) == 0;
}
// --------------------------------------------------------------------

inline bool iequals(const BufferRef& a, const BufferRef& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), a.size()) == 0;
}

template<typename PodType, std::size_t N>
bool iequals(const BufferRef& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	return strncasecmp(a.data(), b, bsize) == 0;
}


inline bool iequals(const BufferRef& a, const std::string& b)
{
	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), b.size()) == 0;
}
// ------------------------------------------------------------------------
inline bool operator==(const BufferRef& a, const BufferRef& b)
{
	return equals(a, b);
}

inline bool operator==(const BufferRef& a, const char *b)
{
	return equals(a, b);
}

template<typename PodType, std::size_t N> inline bool operator==(const BufferRef& a, PodType (&b)[N])
{
	return equals<PodType, N>(a, b);
}
// }}}

//@}

} // namespace x0

#endif
