/* <x0/buffer.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_buffer_hpp
#define sw_x0_buffer_hpp (1)

#include <cstddef>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <string>
#include <stdexcept>

namespace x0 {

//! \addtogroup base
//@{

/**
 * \brief defines a memory buffer construction and access API.
 *
 * This class should be used when sequentially creating and reading parts from it is the main goal
 * of some certain linear information to share.
 */
class buffer
{
public:
	class view;

	typedef char value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;

	static const std::size_t CHUNK_SIZE = 4096;

private:
	value_type *data_;
	std::size_t size_;
	std::size_t capacity_;
	bool readonly_;

#if !defined(NDEBUG) // {{{ reference counting
private:
	mutable std::size_t refcount_;

public:
	void ref() const
	{
		++refcount_;
	}

	void unref() const
	{
		--refcount_;
	}

	std::size_t refcount() const
	{
		return refcount_;
	}
#endif // }}}

public:
	buffer();
	explicit buffer(std::size_t _capacity);
	buffer(const value_type *_data, std::size_t _size); // XXX better be private?
	explicit buffer(const buffer::view& v);
	template<typename PodType, std::size_t N> explicit buffer(PodType (&value)[N]);
	buffer(const buffer& v);
	buffer& operator=(const buffer& v);
	~buffer();

	// attributes
	const value_type *data() const;

	bool empty() const;
	std::size_t size() const;
	void size(std::size_t value);

	std::size_t capacity() const;
	void capacity(std::size_t value);

	void reserve(std::size_t value);
	void clear();

	operator bool() const;
	bool operator!() const;

	// iterator access
	iterator begin();
	iterator end();

	const_iterator cbegin() const;
	const_iterator cend() const;

	// buffer builders
	void push_back(value_type value);
	void push_back(const value_type *value);
	void push_back(const std::string& value);
	void push_back(const void *value, std::size_t size);
	template<typename PodType, std::size_t N> void push_back(PodType (&value)[N]);

	// random access
	value_type& operator[](std::size_t index);
	const value_type& operator[](std::size_t index) const;

	// buffer views
	view sub(std::size_t offset = 0) const;
	view sub(std::size_t offset, std::size_t count) const;

	view operator()(std::size_t offset = 0) const;
	view operator()(std::size_t offset, std::size_t count) const;

	// STL string
	const value_type *c_str() const;
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;

private:
	void assertMutable();
};

class const_buffer : public buffer
{
public:
	template<typename PodType, std::size_t N>
	explicit const_buffer(PodType (&value)[N]);
};

class buffer::view
{
public:
	typedef char value_type;
	typedef const value_type * iterator;

	static const std::size_t npos = std::size_t(-1);

private:
	const buffer *buffer_;
	std::size_t offset_;
	std::size_t size_;

public:
	view();
	view(const buffer *_buffer, std::size_t _offset, std::size_t _size);
	~view();

	view(const buffer& v);
	view(const view& v);

	view& operator=(const buffer& v);
	view& operator=(const view& v);

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

	// find
	std::size_t find(const view& value) const;
	std::size_t find(const value_type *value) const;
	std::size_t find(value_type value) const;

	// rfind
	std::size_t rfind(const view& value) const;
	std::size_t rfind(const value_type *value) const;
	std::size_t rfind(value_type value) const;

	// begins / ibegins
	bool begins(const view& value) const;
	bool begins(const value_type *value) const;
	bool begins(value_type value) const;

	bool ibegins(const view& value) const;
	bool ibegins(const value_type *value) const;
	bool ibegins(value_type value) const;

	// ends / iends
	bool ends(const view& value) const;
	bool ends(const value_type *value) const;
	bool ends(value_type value) const;

	bool iends(const view& value) const;
	bool iends(const value_type *value) const;
	bool iends(value_type value) const;

	// sub
	view sub(std::size_t offset = 0) const;
	view sub(std::size_t offset, std::size_t size) const;

	view operator()(std::size_t offset = 0) const;
	view operator()(std::size_t offset, std::size_t count) const;

	// random access
	const value_type& operator[](std::size_t offset) const;

	// cloning
	buffer clone() const;

	// STL string
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;
};

bool equals(const buffer& a, const buffer& b);
bool equals(const buffer& a, const buffer::view& b);
bool equals(const buffer& a, const char *b);
template<typename PodType, std::size_t N> bool equals(const buffer& a, PodType (&b)[N]);

bool equals(const buffer::view& a, const buffer::view& b);
bool equals(const buffer::view& a, const std::string& b);
template<typename PodType, std::size_t N> bool equals(const buffer::view& a, PodType (&b)[N]);

bool iequals(const buffer& a, const buffer& b);
bool iequals(const buffer& a, const buffer::view& b);
bool iequals(const buffer& a, const char *b);
template<typename PodType, std::size_t N> bool iequals(const buffer& a, PodType (&b)[N]);

bool iequals(const buffer::view& a, const buffer::view& b);
bool iequals(const buffer::view& a, const std::string& b);
template<typename PodType, std::size_t N> bool iequals(const buffer::view& a, PodType (&b)[N]);

bool operator==(const buffer& a, const char *b);
bool operator==(const buffer::view& a, const char *b);

// {{{ buffer impl
inline buffer::buffer() :
	data_(0), size_(0), capacity_(0), readonly_(false)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
}

inline buffer::buffer(std::size_t _capacity) :
	data_(0), size_(0), capacity_(0), readonly_(false)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	reserve(_capacity);
}

inline buffer::buffer(const value_type *_data, std::size_t _size) :
	data_(const_cast<value_type *>(_data)), size_(_size), capacity_(_size), readonly_(true)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
}

inline buffer::buffer(const buffer::view& v) :
	data_(0), size_(0), capacity_(0), readonly_(false)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	push_back(v.data(), v.size());
}

template<typename PodType, std::size_t N>
inline buffer::buffer(PodType (&value)[N]) :
	data_(const_cast<char *>(value)), size_(N - 1), capacity_(N - 1), readonly_(true)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
}

inline buffer::buffer(const buffer& v) :
	data_(0), size_(0), capacity_(0), readonly_(false)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	push_back(v.data(), v.size());
}

inline buffer& buffer::operator=(const buffer& v)
{
	clear();
	push_back(v.data(), v.size());

	return *this;
}

inline buffer::~buffer()
{
	if (data_ && !readonly_)
	{
		std::free(data_);

#ifndef NDEBUG
		size_ = 0;
		capacity_ = 0;
		data_ = 0;

		assert(refcount() == 0 && "No buffer views may reference its buffer object when killing it.");
#endif
	}
}

inline void buffer::assertMutable()
{
#if !defined(NDEBUG)
	if (readonly_)
	{
		throw std::runtime_error("attempted to modify readonly buffer");
	}
#endif
}

inline const buffer::value_type *buffer::data() const
{
	return data_;
}

inline bool buffer::empty() const
{
	return !size_;
}

inline const buffer::value_type *buffer::c_str() const
{
	const_cast<buffer *>(this)->reserve(size_ + 1);
	const_cast<buffer *>(this)->data_[size_] = '\0';

	return data_;
}

inline std::size_t buffer::size() const
{
	return size_;
}

inline void buffer::size(std::size_t value)
{
	if (value > capacity_)
		reserve(value);

	size_ = value;
}

inline std::size_t buffer::capacity() const
{
	return capacity_;
}

inline void buffer::capacity(std::size_t value)
{
	assertMutable();

	capacity_ = value;

	if (size_ > capacity_)
	{
		size_ = capacity_;
	}

	if (capacity_)
	{
		data_ = static_cast<value_type *>(std::realloc(data_, capacity_));
	}
	else if (data_)
	{
		std::free(data_);
	}
}

inline void buffer::reserve(std::size_t value)
{
	if (value > capacity_)
	{
		capacity(value + CHUNK_SIZE - (value % CHUNK_SIZE));
	}
}

inline void buffer::clear()
{
	size(0);
}

inline buffer::operator bool() const
{
	return !empty();
}

inline bool buffer::operator!() const
{
	return empty();
}

inline buffer::iterator buffer::begin()
{
	assertMutable();
	return data_;
}

inline buffer::iterator buffer::end()
{
	assertMutable();
	return data_ + size_;
}

inline buffer::const_iterator buffer::cbegin() const
{
	return data_;
}

inline buffer::const_iterator buffer::cend() const
{
	return data_ + size_;
}

inline void buffer::push_back(value_type value)
{
	reserve(size_ + sizeof(value));

	data_[size_++] = value;
}

inline void buffer::push_back(const value_type *value)
{
	if (std::size_t len = std::strlen(value))
	{
		reserve(size_ + len);

		std::memcpy(end(), value, len);
		size_ += len;
	}
}

inline void buffer::push_back(const std::string& value)
{
	if (std::size_t len = value.size())
	{
		reserve(size_ + len);

		std::memcpy(end(), value.data(), len);
		size_ += len;
	}
}

inline void buffer::push_back(const void *value, std::size_t size)
{
	if (size)
	{
		reserve(size_ + size);

		std::memcpy(end(), value, size);
		size_ += size;
	}
}

template<typename PodType, std::size_t N>
inline void buffer::push_back(PodType (&value)[N])
{
	push_back(reinterpret_cast<const void *>(value), N - 1);
}

inline buffer::value_type& buffer::operator[](std::size_t index)
{
	assert(index >= 0);
	assert(index < size_);

	return data_[index];
}

inline const buffer::value_type& buffer::operator[](std::size_t index) const
{
	assert(index >= 0 && index < size_);

	return data_[index];
}

inline std::string buffer::str() const
{
	return std::string(data_, data_ + size_);
}

inline buffer::view buffer::sub(std::size_t offset) const
{
	assert(offset <= size_);

	return view(this, offset, size_ - offset);
}

inline buffer::view buffer::sub(std::size_t offset, std::size_t count) const
{
	assert(offset >= 0);
	assert(offset + count <= size_);

	return count > 0
		? view(this, offset, count)
		: view();
}

inline buffer::view buffer::operator()(std::size_t offset) const
{
	assert(offset >= 0);
	assert(offset <= size_);

	return view(this, offset, size_ - offset);
}

inline buffer::view buffer::operator()(std::size_t offset, std::size_t count) const
{
	assert(offset >= 0);
	assert(offset + count < size_);

	return view(this, offset, count);
}

inline std::string buffer::substr(std::size_t offset) const
{
	return std::string(data_ + offset, std::min(offset, size_));
}

inline std::string buffer::substr(std::size_t offset, std::size_t count) const
{
	return std::string(data_ + offset, data_ + std::min(offset + count, size_));
}
// }}}

// {{{ const_buffer impl
template<typename PodType, std::size_t N>
inline const_buffer::const_buffer(PodType (&value)[N]) :
	buffer(value)
{
}
// }}}

// {{{ buffer::view impl
inline buffer::view::view() :
	buffer_(0), offset_(0), size_(0)
{
}

inline buffer::view::view(const buffer *_buffer, std::size_t _offset, std::size_t _size) :
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

inline buffer::view::~view()
{
#if !defined(NDEBUG)
	if (buffer_)
		buffer_->unref();
#endif
}

inline buffer::view::view(const buffer& v) :
	buffer_(&v), offset_(0), size_(v.size_)
{
#if !defined(NDEBUG)
	buffer_->ref();
#endif
}

inline buffer::view::view(const view& v) :
	buffer_(v.buffer_), offset_(v.offset_), size_(v.size_)
{
#if !defined(NDEBUG)
	buffer_->ref();
#endif
}

inline buffer::view::view& buffer::view::operator=(const buffer& v)
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

inline buffer::view::view& buffer::view::operator=(const view& v)
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

inline bool buffer::view::empty() const
{
	return !size_;
}

inline std::size_t buffer::view::offset() const
{
	return offset_;
}

inline std::size_t buffer::view::size() const
{
	return size_;
}

inline const buffer::value_type *buffer::view::data() const
{
	return buffer_->data() + offset_;
}

inline buffer::view::operator bool() const
{
	return !empty();
}

inline bool buffer::view::operator!() const
{
	return empty();
}

inline buffer::view::iterator buffer::view::begin() const
{
	return buffer_ ? buffer_->data() + offset_ : 0;
}

inline buffer::view::iterator buffer::view::end() const
{
	return buffer_ ? buffer_->data() + offset_ + size_ : 0;
}

inline std::size_t buffer::view::find(const value_type *value) const
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

inline std::size_t buffer::view::find(value_type value) const
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

inline std::size_t buffer::view::rfind(const value_type *value) const
{
	assert(0 && "not implemented");
	return npos;
}

inline std::size_t buffer::view::rfind(value_type value) const
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

inline bool buffer::view::begins(const view& value) const
{
	return memcmp(data(), value.data(), std::min(size(), value.size())) == 0;
}

inline bool buffer::view::begins(const value_type *value) const
{
	if (!value)
		return true;

	return memcmp(data(), value, std::min(size(), std::strlen(value))) == 0;
}

inline bool buffer::view::begins(value_type value) const
{
	return size() >= 1 && data()[0] == value;
}

inline buffer::view::view buffer::view::sub(std::size_t offset) const
{
	return buffer_->sub(offset_ + offset, size_ - offset);
}

inline buffer::view::view buffer::view::sub(std::size_t offset, std::size_t size) const
{
	return buffer_->sub(offset_ + offset, size);
}

inline buffer::view buffer::view::operator()(std::size_t offset) const
{
	return buffer_->sub(offset_ + offset, size_ - offset);
}

inline buffer::view buffer::view::operator()(std::size_t offset, std::size_t count) const
{
	return buffer_->sub(offset_ + offset, count);
}

inline const buffer::value_type& buffer::view::operator[](std::size_t offset) const
{
	return data()[offset];
}

inline buffer buffer::view::clone() const
{
	buffer buf(size_);
	buf.push_back(data(), size_);

	return buf;
}

inline std::string buffer::view::str() const
{
	return substr(0);
}

inline std::string buffer::view::substr(std::size_t offset) const
{
	return std::string(data() + offset, size_ - std::min(offset, size_));
}

inline std::string buffer::view::substr(std::size_t offset, std::size_t count) const
{
	return std::string(data() + offset, std::min(count, size_));
}
// }}}

// {{{ free function impl
inline bool equals(const buffer& a, const buffer& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool equals(const buffer& a, const buffer::view& b)
{
	return a.size() == b.size()
		&& (a.data() == b.data() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

inline bool equals(const buffer& a, const char *b)
{
	std::size_t bsize = std::strlen(b);

	return a.size() == bsize
		&& (a.data() == b || std::memcmp(a.data(), b, bsize) == 0);
}

template<typename PodType, std::size_t N>
bool equals(const buffer& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	if (a.data() == b)
		return true;

	return std::memcmp(a.data(), b, bsize) == 0;
}

// buffer::view
inline bool equals(const buffer::view& a, const buffer::view& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool equals(const buffer::view& a, const std::string& b)
{
	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

template<typename PodType, std::size_t N>
bool equals(const buffer::view& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	return std::memcmp(a.data(), b, bsize) == 0;
}

// --------------------------------------------------------------------
inline bool iequals(const buffer& a, const buffer& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return a.data() == b.data() || strncasecmp(a.data(), b.data(), a.size()) == 0;
}

inline bool iequals(const buffer& a, const buffer::view& b)
{
	if (a.size() != b.size())
		return false;

	if (strncasecmp(a.data(), b.data(), b.size()) != 0)
		return false;

	return true;
}

inline bool iequals(const buffer& a, const char *b)
{
	std::size_t bsize = b ? std::strlen(b) : 0;

	if (a.size() != bsize)
		return false;

	if (strncasecmp(a.data(), b, bsize) != 0)
		return false;

	return true;
}

template<typename PodType, std::size_t N>
bool iequals(const buffer& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	if (strncasecmp(a.data(), b, bsize) != 0)
		return false;

	return true;
}

//.
inline bool iequals(const buffer::view& a, const buffer::view& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), a.size()) == 0;
}

inline bool iequals(const buffer::view& a, const std::string& b)
{
	if (a.size() != b.size())
		return false;

	return strncasecmp(a.data(), b.data(), a.size()) == 0;
}

template<typename PodType, std::size_t N>
bool iequals(const buffer::view& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	return strncasecmp(a.data(), b, bsize) == 0;
}

// ------------------------------------------------------------------------
inline bool operator==(const x0::buffer& a, const x0::buffer& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer& a, const x0::buffer::view& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer& a, const char *b)
{
	return equals(a, b);
}

template<typename PodType, std::size_t N> bool operator==(const buffer& a, PodType (&b)[N])
{
	return equals<PodType, N>(a, b);
}

// buffer::view
inline bool operator==(const x0::buffer::view& a, const x0::buffer& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer::view& a, const x0::buffer::view& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::buffer::view& a, const char *b)
{
	return equals(a, b);
}

template<typename PodType, std::size_t N> bool operator==(const buffer::view& a, PodType (&b)[N])
{
	return equals<PodType, N>(a, b);
}
// }}}

//@}

} // namespace x0

#endif
