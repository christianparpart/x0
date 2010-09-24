/* <x0/BufferRef.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_BufferRef_h
#define sw_x0_BufferRef_h (1)

#include <x0/Buffer.h>

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

private:
	const Buffer *buffer_;
	std::size_t offset_;
	std::size_t size_;

public:
	BufferRef();
	BufferRef(const Buffer *_buffer, std::size_t _offset, std::size_t _size);
	~BufferRef();

	BufferRef(const Buffer& v);
	BufferRef(const BufferRef& v);

	BufferRef& operator=(const Buffer& v);
	BufferRef& operator=(const BufferRef& v);

	void clear();

	// properties
	bool empty() const;
	std::size_t offset() const;
	std::size_t size() const;
	const value_type *data() const;
	Buffer& buffer() const;

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
//	std::size_t find(const value_type *value, std::size_t offset = 0) const;
	std::size_t find(const BufferRef& value, std::size_t offset = 0) const;
	std::size_t find(value_type value, std::size_t offset = 0) const;

	// rfind
	template<typename PodType, std::size_t N> std::size_t rfind(PodType (&value)[N]) const;
	std::size_t rfind(const value_type *value) const;
	std::size_t rfind(const BufferRef& value) const;
	std::size_t rfind(value_type value) const;

	// begins / ibegins
	bool begins(const std::string& value) const;
	bool begins(const BufferRef& value) const;
	bool begins(const value_type *value) const;
	bool begins(value_type value) const;

	bool ibegins(const std::string& value) const;
	bool ibegins(const BufferRef& value) const;
	bool ibegins(const value_type *value) const;
	bool ibegins(value_type value) const;

	// ends / iends
	bool ends(const std::string& value) const;
	bool ends(const BufferRef& value) const;
	bool ends(const value_type *value) const;
	bool ends(value_type value) const;

	bool iends(const std::string& value) const;
	bool iends(const BufferRef& value) const;
	bool iends(const value_type *value) const;
	bool iends(value_type value) const;

	// sub
	BufferRef ref(std::size_t offset = 0) const;
	BufferRef ref(std::size_t offset, std::size_t size) const;

	BufferRef operator()(std::size_t offset = 0) const;
	BufferRef operator()(std::size_t offset, std::size_t count) const;

	// random access
	const value_type& operator[](std::size_t offset) const;

	// cloning
	Buffer clone() const;

	// STL string
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;

	// casts
	template<typename T> T as() const;
	template<typename T> T hex() const;

	bool toBool() const;
	int toInt() const;
};

// free functions
bool equals(const Buffer& a, const BufferRef& b);
bool equals(const BufferRef& a, const BufferRef& b);
bool equals(const BufferRef& a, const std::string& b);
template<typename PodType, std::size_t N> bool equals(const BufferRef& a, PodType (&b)[N]);

bool iequals(const BufferRef& a, const BufferRef& b);
bool iequals(const Buffer& a, const BufferRef& b);
bool iequals(const BufferRef& a, const std::string& b);
template<typename PodType, std::size_t N> bool iequals(const BufferRef& a, PodType (&b)[N]);

bool operator==(const BufferRef& a, const BufferRef& b);
bool operator==(const BufferRef& a, const Buffer& b);
bool operator==(const Buffer& a, const BufferRef& b);
bool operator==(const BufferRef& a, const char *b);
template<typename PodType, std::size_t N> bool operator==(const BufferRef& a, PodType (&b)[N]);

std::string operator+(const BufferRef& a, const std::string& b);
std::string operator+(const std::string& a, const BufferRef& b);

// {{{ BufferRef impl
inline BufferRef::BufferRef() :
	buffer_(0), offset_(0), size_(0)
{
}

inline BufferRef::BufferRef(const Buffer *_buffer, std::size_t _offset, std::size_t _size) :
	buffer_(_buffer), offset_(_offset), size_(_size)
{
#if !defined(NDEBUG)
	if (buffer_)
	{
		assert(offset_ + size_ <= buffer_->size());
	}
	else
	{
		assert(!offset_);
		assert(!size_);
	}
#endif
}

inline BufferRef::~BufferRef()
{
}

inline BufferRef::BufferRef(const Buffer& v) :
	buffer_(&v), offset_(0), size_(v.size_)
{
}

inline BufferRef::BufferRef(const BufferRef& v) :
	buffer_(v.buffer_), offset_(v.offset_), size_(v.size_)
{
}

inline BufferRef& BufferRef::operator=(const Buffer& v)
{
	buffer_ = &v;
	offset_ = 0;
	size_ = v.size_;

	return *this;
}

inline BufferRef& BufferRef::operator=(const BufferRef& v)
{
	buffer_ = v.buffer_;
	offset_ = v.offset_;
	size_ = v.size_;

	return *this;
}

inline void BufferRef::clear()
{
	size_ = 0;
}

inline bool BufferRef::empty() const
{
	return !size_;
}

inline std::size_t BufferRef::offset() const
{
	return offset_;
}

inline std::size_t BufferRef::size() const
{
	return size_;
}

inline const Buffer::value_type *BufferRef::data() const
{
	return buffer_ ? buffer_->data() + offset_ : NULL;
}

inline Buffer& BufferRef::buffer() const
{
	assert(buffer_ != 0);
	return *const_cast<Buffer *>(buffer_);
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
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ : 0;
}

inline BufferRef::iterator BufferRef::end() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ + size_ : 0;
}

inline BufferRef::const_iterator BufferRef::cbegin() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ : 0;
}

inline BufferRef::const_iterator BufferRef::cend() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ + size_ : 0;
}

/** shifts view's left margin by given bytes to the left, thus, increasing view's size.
 */
inline void BufferRef::shl(ssize_t offset)
{
	assert(buffer_ != 0);

	offset_ -= offset;
	size_ += offset;

	assert(offset_ >= 0);
	assert(size_ >= 0);
	assert(offset_ + size_ <= buffer_->capacity());
}

/** shifts view's right margin by given bytes to the right, thus, increasing view's size.
 */
inline void BufferRef::shr(ssize_t offset)
{
	assert(buffer_ != 0);

	size_ += offset;

	assert(offset_ + size_ <= buffer_->capacity());
}

#if 0
inline std::size_t BufferRef::find(const value_type *value, std::size_t offset) const
{
	const char *i = data() + offset;
	const char *e = i + size() - offset;
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
#endif

inline std::size_t BufferRef::find(value_type value, std::size_t offset) const
{
	if (const char *p = strchr(data() + offset, value))
	{
		if (p < end())
		{
			return p - data();
		}
	}
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
	if (!buffer_)
		return npos;

	const char *p = data();
	const char *q = p + size();

	while (p != q)
	{
		if (*q == value)
		{
			return q - p;
		}
		--q;
	}

	return *p == value ? 0 : npos;
}

template<typename PodType, std::size_t N>
std::size_t BufferRef::rfind(PodType (&value)[N]) const
{
	if (!buffer_)
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

inline bool BufferRef::begins(const std::string& value) const
{
	return memcmp(data(), value.data(), std::min(size(), value.size())) == 0;
}

inline bool BufferRef::begins(const BufferRef& value) const
{
	return memcmp(data(), value.data(), std::min(size(), value.size())) == 0;
}

inline bool BufferRef::begins(const value_type *value) const
{
	if (!value)
		return true;

	return memcmp(data(), value, std::min(size(), std::strlen(value))) == 0;
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
	assert(buffer_ != 0);

	if (!value)
		return true;

	size_t valueLength = std::strlen(value);

	if (size() < valueLength)
		return false;

	return memcmp(data() + size() - valueLength, value, valueLength) == 0;
}

inline BufferRef BufferRef::ref(std::size_t offset) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, size_ - offset);
}

inline BufferRef BufferRef::ref(std::size_t offset, std::size_t size) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, size);
}

inline BufferRef BufferRef::operator()(std::size_t offset) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, size_ - offset);
}

inline BufferRef BufferRef::operator()(std::size_t offset, std::size_t count) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, count);
}

inline const Buffer::value_type& BufferRef::operator[](std::size_t offset) const
{
	return data()[offset];
}

inline Buffer BufferRef::clone() const
{
	if (!size_)
		return Buffer();

	Buffer buf(size_);
	buf.push_back(data(), size_);

	return buf;
}

inline std::string BufferRef::str() const
{
	if (size_)
	{
		assert(buffer_ != 0);
		return substr(0);
	}
	return std::string();
}

inline std::string BufferRef::substr(std::size_t offset) const
{
	assert(buffer_ != 0);
	assert(offset <= size_);
	return std::string(data() + offset, size_ - std::min(offset, size_));
}

inline std::string BufferRef::substr(std::size_t offset, std::size_t count) const
{
	assert(buffer_ != 0);
	assert(offset + count <= size_);
	return std::string(data() + offset, std::min(count, size_));
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
// }}}

// {{{ free function impl
inline bool equals(const Buffer& a, const BufferRef& b)
{
	return a.size() == b.size()
		&& (a.data() == b.data() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

inline bool equals(const BufferRef& a, const BufferRef& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool equals(const BufferRef& a, const std::string& b)
{
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

// --------------------------------------------------------------------
inline bool iequals(const Buffer& a, const BufferRef& b)
{
	if (a.size() != b.size())
		return false;

	if (strncasecmp(a.data(), b.data(), b.size()) != 0)
		return false;

	return true;
}

//.
inline bool iequals(const BufferRef& a, const BufferRef& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), a.size()) == 0;
}

inline bool iequals(const BufferRef& a, const std::string& b)
{
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

// ------------------------------------------------------------------------
inline bool operator==(const BufferRef& a, const BufferRef& b)
{
	return equals(a, b);
}

inline bool operator==(const BufferRef& a, const Buffer& b)
{
	return equals(a, b);
}

inline bool operator==(const Buffer& a, const BufferRef& b)
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

inline std::string operator+(const BufferRef& a, const std::string& b)
{
	return std::string(a.data(), a.size()) + b;
}

inline std::string operator+(const std::string& a, const BufferRef& b)
{
	return a + std::string(b.data(), b.size());
}
// }}}

//@}

} // namespace x0

#endif
