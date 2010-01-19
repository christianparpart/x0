/* <x0/buffer_ref.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009-2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_buffer_ref_hpp
#define sw_x0_buffer_ref_hpp (1)

#include <x0/buffer.hpp>
#include <cassert>
#include <cstring>
#include <cstddef>
#include <climits>

#include <cstdlib>
#include <string>
#include <stdexcept>

#if !defined(X0_BUFFER_NO_ASIO)
#	include <asio/buffer.hpp> // mutable_buffer
#endif

namespace x0 {

//! \addtogroup base
//@{

/** holds a reference to a region of a buffer
 */
class buffer_ref
{
public:
	typedef char value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;

	static const std::size_t npos = std::size_t(-1);

private:
	const buffer *buffer_;
	std::size_t offset_;
	std::size_t size_;

public:
	buffer_ref();
	buffer_ref(const buffer *_buffer, std::size_t _offset, std::size_t _size);
	~buffer_ref();

	buffer_ref(const buffer& v);
	buffer_ref(const buffer_ref& v);

	buffer_ref& operator=(const buffer& v);
	buffer_ref& operator=(const buffer_ref& v);

	// properties
	bool empty() const;
	std::size_t offset() const;
	std::size_t size() const;
	const value_type *data() const;

	operator bool() const;
	bool operator!() const;

	// iterator access
	iterator begin() const;
	iterator end() const;

	const_iterator cbegin() const;
	const_iterator cend() const;

	// find
	std::size_t find(const buffer_ref& value) const;
	std::size_t find(const value_type *value) const;
	std::size_t find(value_type value) const;

	// rfind
	std::size_t rfind(const buffer_ref& value) const;
	std::size_t rfind(const value_type *value) const;
	std::size_t rfind(value_type value) const;

	// begins / ibegins
	bool begins(const buffer_ref& value) const;
	bool begins(const value_type *value) const;
	bool begins(value_type value) const;

	bool ibegins(const buffer_ref& value) const;
	bool ibegins(const value_type *value) const;
	bool ibegins(value_type value) const;

	// ends / iends
	bool ends(const buffer_ref& value) const;
	bool ends(const value_type *value) const;
	bool ends(value_type value) const;

	bool iends(const buffer_ref& value) const;
	bool iends(const value_type *value) const;
	bool iends(value_type value) const;

	// sub
	buffer_ref ref(std::size_t offset = 0) const;
	buffer_ref ref(std::size_t offset, std::size_t size) const;

	buffer_ref operator()(std::size_t offset = 0) const;
	buffer_ref operator()(std::size_t offset, std::size_t count) const;

	// random access
	const value_type& operator[](std::size_t offset) const;

	// cloning
	buffer clone() const;

	// STL string
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;

public: // ASIO support
#if !defined(X0_BUFFER_NO_ASIO)
	/** casts this buffer_ref to asio::mutable_buffer.
	 */
	operator asio::mutable_buffer() { return asio::mutable_buffer(begin(), sizeof(value_type) * size()); }

	/** cats this buffer_ref to asio::mutable_buffers_1 as required by asio's read/write functions.
	 */
	asio::mutable_buffers_1 asio_buffer() { return asio::mutable_buffers_1(asio::mutable_buffer(begin(), sizeof(value_type) * size())); }

#endif
};

// free functions
bool equals(const buffer& a, const buffer_ref& b);
bool equals(const buffer_ref& a, const buffer_ref& b);
bool equals(const buffer_ref& a, const std::string& b);
template<typename PodType, std::size_t N> bool equals(const buffer_ref& a, PodType (&b)[N]);

bool iequals(const buffer_ref& a, const buffer_ref& b);
bool iequals(const buffer& a, const buffer_ref& b);
bool iequals(const buffer_ref& a, const std::string& b);
template<typename PodType, std::size_t N> bool iequals(const buffer_ref& a, PodType (&b)[N]);

bool operator==(const x0::buffer_ref& a, const x0::buffer_ref& b);
bool operator==(const x0::buffer_ref& a, const x0::buffer& b);
bool operator==(const x0::buffer& a, const x0::buffer_ref& b);
bool operator==(const buffer_ref& a, const char *b);
template<typename PodType, std::size_t N> bool operator==(const buffer_ref& a, PodType (&b)[N]);

// {{{ buffer_ref impl
inline buffer_ref::buffer_ref() :
	buffer_(0), offset_(0), size_(0)
{
}

inline buffer_ref::buffer_ref(const buffer *_buffer, std::size_t _offset, std::size_t _size) :
	buffer_(_buffer), offset_(_offset), size_(_size)
{
#if !defined(NDEBUG)
	if (buffer_)
	{
		buffer_->ref();
		assert(offset_ + size_ <= buffer_->size());
	}
	else
	{
		assert(!offset_);
		assert(!size_);
	}
#endif
}

inline buffer_ref::~buffer_ref()
{
#if !defined(NDEBUG)
	if (buffer_)
		buffer_->unref();
#endif
}

inline buffer_ref::buffer_ref(const buffer& v) :
	buffer_(&v), offset_(0), size_(v.size_)
{
#if !defined(NDEBUG)
	buffer_->ref();
#endif
}

inline buffer_ref::buffer_ref(const buffer_ref& v) :
	buffer_(v.buffer_), offset_(v.offset_), size_(v.size_)
{
#if !defined(NDEBUG)
	buffer_->ref();
#endif
}

inline buffer_ref::buffer_ref& buffer_ref::operator=(const buffer& v)
{
#if !defined(NDEBUG)
	if (buffer_)
		buffer_->unref();
#endif

	buffer_ = &v;
	offset_ = 0;
	size_ = v.size_;

#if !defined(NDEBUG)
	buffer_->ref();
#endif

	return *this;
}

inline buffer_ref::buffer_ref& buffer_ref::operator=(const buffer_ref& v)
{
#if !defined(NDEBUG)
	if (buffer_)
		buffer_->unref();
#endif

	buffer_ = v.buffer_;
	offset_ = v.offset_;
	size_ = v.size_;

#if !defined(NDEBUG)
	if (buffer_)
		buffer_->ref();
#endif

	return *this;
}

inline bool buffer_ref::empty() const
{
	return !size_;
}

inline std::size_t buffer_ref::offset() const
{
	return offset_;
}

inline std::size_t buffer_ref::size() const
{
	return size_;
}

inline const buffer::value_type *buffer_ref::data() const
{
	return buffer_ ? buffer_->data() + offset_ : NULL;
}

inline buffer_ref::operator bool() const
{
	return !empty();
}

inline bool buffer_ref::operator!() const
{
	return empty();
}

inline buffer_ref::iterator buffer_ref::begin() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ : 0;
}

inline buffer_ref::iterator buffer_ref::end() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ + size_ : 0;
}

inline buffer_ref::const_iterator buffer_ref::cbegin() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ : 0;
}

inline buffer_ref::const_iterator buffer_ref::cend() const
{
	return buffer_ ? const_cast<value_type *>(buffer_->data()) + offset_ + size_ : 0;
}

inline std::size_t buffer_ref::find(const value_type *value) const
{
	if (const char *p = strstr(data(), value))
	{
		if (p < end())
		{
			return p - data();
		}
	}
	return npos;
}

inline std::size_t buffer_ref::find(value_type value) const
{
	if (const char *p = strchr(data(), value))
	{
		if (p < end())
		{
			return p - data();
		}
	}
	return npos;
}

inline std::size_t buffer_ref::rfind(const value_type *value) const
{
	assert(0 && "not implemented");
	return npos;
}

inline std::size_t buffer_ref::rfind(value_type value) const
{
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

inline bool buffer_ref::begins(const buffer_ref& value) const
{
	return memcmp(data(), value.data(), std::min(size(), value.size())) == 0;
}

inline bool buffer_ref::begins(const value_type *value) const
{
	if (!value)
		return true;

	return memcmp(data(), value, std::min(size(), std::strlen(value))) == 0;
}

inline bool buffer_ref::begins(value_type value) const
{
	return size() >= 1 && data()[0] == value;
}

inline buffer_ref::buffer_ref buffer_ref::ref(std::size_t offset) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, size_ - offset);
}

inline buffer_ref::buffer_ref buffer_ref::ref(std::size_t offset, std::size_t size) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, size);
}

inline buffer_ref buffer_ref::operator()(std::size_t offset) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, size_ - offset);
}

inline buffer_ref buffer_ref::operator()(std::size_t offset, std::size_t count) const
{
	assert(buffer_ != 0);
	return buffer_->ref(offset_ + offset, count);
}

inline const buffer::value_type& buffer_ref::operator[](std::size_t offset) const
{
	return data()[offset];
}

inline buffer buffer_ref::clone() const
{
	if (!size_)
		return buffer();

	buffer buf(size_);
	buf.push_back(data(), size_);

	return buf;
}

inline std::string buffer_ref::str() const
{
	assert(buffer_ != 0);
	return substr(0);
}

inline std::string buffer_ref::substr(std::size_t offset) const
{
	assert(buffer_ != 0);
	return std::string(data() + offset, size_ - std::min(offset, size_));
}

inline std::string buffer_ref::substr(std::size_t offset, std::size_t count) const
{
	assert(buffer_ != 0);
	return std::string(data() + offset, std::min(count, size_));
}
// }}}

// {{{ free function impl
inline bool equals(const buffer& a, const buffer_ref& b)
{
	return a.size() == b.size()
		&& (a.data() == b.data() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

inline bool equals(const buffer_ref& a, const buffer_ref& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool equals(const buffer_ref& a, const std::string& b)
{
	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

template<typename PodType, std::size_t N>
bool equals(const buffer_ref& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	return std::memcmp(a.data(), b, bsize) == 0;
}

// --------------------------------------------------------------------
inline bool iequals(const buffer& a, const buffer_ref& b)
{
	if (a.size() != b.size())
		return false;

	if (strncasecmp(a.data(), b.data(), b.size()) != 0)
		return false;

	return true;
}

//.
inline bool iequals(const buffer_ref& a, const buffer_ref& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), a.size()) == 0;
}

inline bool iequals(const buffer_ref& a, const std::string& b)
{
	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), a.size()) == 0;
}

template<typename PodType, std::size_t N>
bool iequals(const buffer_ref& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	return strncasecmp(a.data(), b, bsize) == 0;
}

// ------------------------------------------------------------------------
inline bool operator==(const x0::buffer_ref& a, const x0::buffer_ref& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer_ref& a, const x0::buffer& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer& a, const x0::buffer_ref& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer_ref& a, const char *b)
{
	return equals(a, b);
}

template<typename PodType, std::size_t N> inline bool operator==(const buffer_ref& a, PodType (&b)[N])
{
	return equals<PodType, N>(a, b);
}
// }}}

//@}

} // namespace x0

#endif
