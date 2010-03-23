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

class buffer_ref;

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
	typedef char value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;

	typedef buffer_ref view;

	//static const std::size_t CHUNK_SIZE = 4096;
	enum { CHUNK_SIZE = 4096 };

	struct helper { int i; };
	typedef int (helper::*helper_type);

private:
	enum edit_mode_t { EDIT_ALL, EDIT_NO_RESIZE, EDIT_NOTHING };

	friend class buffer_ref;

protected:
	value_type *data_;
	std::size_t size_;
	std::size_t capacity_;

	edit_mode_t edit_mode_;

#if !defined(NDEBUG) // {{{ reference counting
private:
	mutable std::size_t refcount_;

private:
	void _ref() const
	{
		++refcount_;
	}

	void _unref() const
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
	explicit buffer(const buffer_ref& v);
	explicit buffer(const std::string& v);
	template<typename PodType, std::size_t N> explicit buffer(PodType (&value)[N]);
	buffer(const buffer& v);
	buffer(buffer&& v);
	buffer& operator=(buffer&& v);
	buffer& operator=(const buffer& v);
	buffer& operator=(const std::string& v);
	buffer& operator=(const value_type *v);
	~buffer();

	// attributes
	const value_type *data() const;

	bool empty() const;
	std::size_t size() const;
	void resize(std::size_t value);

	std::size_t capacity() const;
	void capacity(std::size_t value);

	void reserve(std::size_t value);
	void clear();

	operator helper_type() const;
	bool operator!() const;

	// iterator access
	iterator begin() const;
	iterator end() const;

	const_iterator cbegin() const;
	const_iterator cend() const;

	// buffer builders
	void push_back(value_type value);
	void push_back(const value_type *value);
	void push_back(const buffer& value);
	void push_back(const buffer_ref& value);
	void push_back(const std::string& value);
	void push_back(const void *value, std::size_t size);
	template<typename PodType, std::size_t N> void push_back(PodType (&value)[N]);

	// random access
	value_type& operator[](std::size_t index);
	const value_type& operator[](std::size_t index) const;

	// buffer views
	buffer_ref ref(std::size_t offset = 0) const;
	buffer_ref ref(std::size_t offset, std::size_t count) const;

	buffer_ref operator()(std::size_t offset = 0) const;
	buffer_ref operator()(std::size_t offset, std::size_t count) const;

	// STL string
	const value_type *c_str() const;
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;

	// statics
	static buffer from_copy(const value_type *data, std::size_t count);

private:
	void assertMutable();
};

class const_buffer : public buffer
{
public:
	template<typename PodType, std::size_t N>
	explicit const_buffer(PodType (&value)[N]);

	const_buffer(const value_type *value, std::size_t n);
};

template<const std::size_t N>
class fixed_buffer : public buffer
{
private:
	value_type fixed_[N];

public:
	fixed_buffer();
};

// free functions
bool equals(const buffer& a, const buffer& b);
bool equals(const buffer& a, const char *b);
template<typename PodType, std::size_t N> bool equals(const buffer& a, PodType (&b)[N]);

bool iequals(const buffer& a, const buffer& b);
bool iequals(const buffer& a, const char *b);
template<typename PodType, std::size_t N> bool iequals(const buffer& a, PodType (&b)[N]);

bool operator==(const buffer& a, const buffer& b);
bool operator==(const buffer& a, const char *b);
template<typename PodType, std::size_t N> bool operator==(const buffer& a, PodType (&b)[N]);

} // namespace x0

//@}

#include <x0/buffer_ref.hpp>

// {{{ impl

namespace x0 {

// {{{ buffer impl
inline buffer::buffer() :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
}

inline buffer::buffer(std::size_t _capacity) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	reserve(_capacity);
}

inline buffer::buffer(const value_type *_data, std::size_t _size) :
	data_(const_cast<value_type *>(_data)), size_(_size), capacity_(_size), edit_mode_(EDIT_NOTHING)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
}

inline buffer::buffer(const buffer_ref& v) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	push_back(v.data(), v.size());
}

inline buffer::buffer(const std::string& v) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	push_back(v.data(), v.size());
}

template<typename PodType, std::size_t N>
inline buffer::buffer(PodType (&value)[N]) :
	data_(const_cast<char *>(value)), size_(N - 1), capacity_(N - 1), edit_mode_(EDIT_NOTHING)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
}

inline buffer::buffer(const buffer& v) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
	push_back(v.data(), v.size());
}

inline buffer::buffer(buffer&& v) :
	data_(v.data_),
	size_(v.size_),
	capacity_(v.capacity_),
	edit_mode_(v.edit_mode_)
#if !defined(NDEBUG)
	, refcount_(0)
#endif
{
#if !defined(NDEBUG)
	assert(v.refcount_ == 0);
#endif

	v.data_ = 0;
	v.size_ = 0;
	v.capacity_ = 0;
	v.edit_mode_ = EDIT_ALL;
}

inline buffer& buffer::operator=(buffer&& v)
{
#if !defined(NDEBUG)
	//sometimes intentional to still have (an unused) ref to this buffer.
//	assert(refcount_ == 0);
//	assert(v.refcount_ == 0);
#endif

	clear();

	data_ = v.data_;
	size_ = v.size_;
	capacity_ = v.capacity_;
	edit_mode_ = v.edit_mode_;

	v.data_ = 0;
	v.size_ = 0;
	v.capacity_ = 0;

	return *this;
}

inline buffer& buffer::operator=(const buffer& v)
{
	clear();
	push_back(v.data(), v.size());

	return *this;
}

inline buffer& buffer::operator=(const std::string& v)
{
	clear();
	push_back(v.data(), v.size());

	return *this;
}

inline buffer& buffer::operator=(const value_type *v)
{
	clear();
	push_back(v, std::strlen(v));

	return *this;
}

inline buffer::~buffer()
{
	if (data_ && edit_mode_ == EDIT_ALL)
	{
		std::free(data_);
#ifndef NDEBUG
		size_ = 0;
		capacity_ = 0;
		data_ = 0;
#endif
	}
}

inline void buffer::assertMutable()
{
#if !defined(NDEBUG)
	switch (edit_mode_)
	{
		case EDIT_ALL:
		case EDIT_NO_RESIZE:
			break;
		default:
			assert(0 == "attempted to modify readonly buffer");
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

inline void buffer::resize(std::size_t value)
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
	if (value == capacity_)
		return;

#if !defined(NDEBUG)
	switch (edit_mode_)
	{
		case EDIT_ALL:
			break;
		case EDIT_NO_RESIZE:
		case EDIT_NOTHING:
		default:
			assert(0 == "attempted to modify readonly buffer");
			throw std::runtime_error("attempted to modify readonly buffer");
	}
#endif

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
	resize(0);
}

inline buffer::operator helper_type() const
{
	return !empty() ? &helper::i : 0;
}

inline bool buffer::operator!() const
{
	return empty();
}

inline buffer::iterator buffer::begin() const
{
	return data_;
}

inline buffer::iterator buffer::end() const
{
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

inline void buffer::push_back(const buffer& value)
{
	if (std::size_t len = value.size())
	{
		reserve(size_ + len);

		std::memcpy(end(), value.begin(), len);
		size_ += len;
	}
}

inline void buffer::push_back(const buffer_ref& value)
{
	if (std::size_t len = value.size())
	{
		reserve(size_ + len);

		std::memcpy(end(), value.begin(), len);
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

inline buffer_ref buffer::ref(std::size_t offset) const
{
	assert(offset <= size_);

	return buffer_ref(this, offset, size_ - offset);
}

inline buffer_ref buffer::ref(std::size_t offset, std::size_t count) const
{
	assert(offset >= 0);
	assert(offset + count <= size_);

	return count > 0
		? buffer_ref(this, offset, count)
		: buffer_ref();
}

inline buffer_ref buffer::operator()(std::size_t offset) const
{
	assert(offset >= 0);
	assert(offset <= size_);

	return buffer_ref(this, offset, size_ - offset);
}

inline buffer_ref buffer::operator()(std::size_t offset, std::size_t count) const
{
	assert(offset >= 0);
	assert(offset + count < size_);

	return buffer_ref(this, offset, count);
}

inline std::string buffer::substr(std::size_t offset) const
{
	return std::string(data_ + offset, std::min(offset, size_));
}

inline std::string buffer::substr(std::size_t offset, std::size_t count) const
{
	return std::string(data_ + offset, data_ + std::min(offset + count, size_));
}

inline buffer buffer::from_copy(const value_type *data, std::size_t count)
{
	buffer result(count);
	result.push_back(data);
	return result;
}
// }}}

// {{{ const_buffer impl
template<typename PodType, std::size_t N>
inline const_buffer::const_buffer(PodType (&value)[N]) :
	buffer(value)
{
}

inline const_buffer::const_buffer(const value_type *value, std::size_t n) :
	buffer(value, n)
{
}
// }}}

// {{{ fixed_buffer impl
template<std::size_t N>
inline fixed_buffer<N>::fixed_buffer() :
	buffer()
{
	data_ = fixed_;
	size_ = 0;
	capacity_ = N;
	edit_mode_ = EDIT_NO_RESIZE;
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

// --------------------------------------------------------------------
inline bool iequals(const buffer& a, const buffer& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return a.data() == b.data() || strncasecmp(a.data(), b.data(), a.size()) == 0;
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

// ------------------------------------------------------------------------
inline bool operator==(const x0::buffer& a, const x0::buffer& b)
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
// }}}

} // namespace x0

// }}}

#endif
