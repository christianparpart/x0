/* <x0/Buffer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
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

class BufferRef;

//! \addtogroup base
//@{

/**
 * \brief defines a memory buffer construction and access API.
 *
 * This class should be used when sequentially creating and reading parts from it is the main goal
 * of some certain linear information to share.
 */
class Buffer
{
public:
	typedef char value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;

	typedef BufferRef view;

	//static const std::size_t CHUNK_SIZE = 4096;
	enum { CHUNK_SIZE = 4096 };

	struct helper { int i; };
	typedef int (helper::*helper_type);

private:
	enum edit_mode_t { EDIT_ALL, EDIT_NO_RESIZE, EDIT_NOTHING };

	friend class BufferRef;

protected:
	value_type *data_;
	std::size_t size_;
	std::size_t capacity_;

	edit_mode_t edit_mode_;

public:
	Buffer();
	explicit Buffer(std::size_t _capacity);
	Buffer(const value_type *_data, std::size_t _size); // XXX better be private?
	explicit Buffer(const BufferRef& v);
	explicit Buffer(const std::string& v);
	template<typename PodType, std::size_t N> explicit Buffer(PodType (&value)[N]);
	Buffer(const Buffer& v);
	Buffer(Buffer&& v);
	Buffer& operator=(Buffer&& v);
	Buffer& operator=(const Buffer& v);
	Buffer& operator=(const std::string& v);
	Buffer& operator=(const value_type *v);
	~Buffer();

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
	void push_back(int value);
	void push_back(const value_type *value);
	void push_back(const Buffer& value);
	void push_back(const BufferRef& value);
	void push_back(const std::string& value);
	void push_back(const void *value, std::size_t size);
	template<typename PodType, std::size_t N> void push_back(PodType (&value)[N]);

	// random access
	value_type& operator[](std::size_t index);
	const value_type& operator[](std::size_t index) const;

	// buffer views
	BufferRef ref(std::size_t offset = 0) const;
	BufferRef ref(std::size_t offset, std::size_t count) const;

	BufferRef operator()(std::size_t offset = 0) const;
	BufferRef operator()(std::size_t offset, std::size_t count) const;

	// STL string
	const value_type *c_str() const;
	std::string str() const;
	std::string substr(std::size_t offset) const;
	std::string substr(std::size_t offset, std::size_t count) const;

	// statics
	static Buffer from_copy(const value_type *data, std::size_t count);

private:
	void assertMutable();
};

class ConstBuffer : public Buffer
{
public:
	template<typename PodType, std::size_t N>
	explicit ConstBuffer(PodType (&value)[N]);

	ConstBuffer(const value_type *value, std::size_t n);
};

template<const std::size_t N>
class FixedBuffer : public Buffer
{
private:
	value_type fixed_[N];

public:
	FixedBuffer();
};

// free functions
bool equals(const Buffer& a, const Buffer& b);
bool equals(const Buffer& a, const char *b);
template<typename PodType, std::size_t N> bool equals(const Buffer& a, PodType (&b)[N]);

bool iequals(const Buffer& a, const Buffer& b);
bool iequals(const Buffer& a, const char *b);
template<typename PodType, std::size_t N> bool iequals(const Buffer& a, PodType (&b)[N]);

bool operator==(const Buffer& a, const Buffer& b);
bool operator==(const Buffer& a, const char *b);
template<typename PodType, std::size_t N> bool operator==(const Buffer& a, PodType (&b)[N]);

} // namespace x0

//@}

#include <x0/BufferRef.h>

// {{{ impl

namespace x0 {

// {{{ Buffer impl
inline Buffer::Buffer() :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
{
}

inline Buffer::Buffer(std::size_t _capacity) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
{
	reserve(_capacity);
}

inline Buffer::Buffer(const value_type *_data, std::size_t _size) :
	data_(const_cast<value_type *>(_data)), size_(_size), capacity_(_size), edit_mode_(EDIT_NOTHING)
{
}

inline Buffer::Buffer(const BufferRef& v) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
{
	push_back(v.data(), v.size());
}

inline Buffer::Buffer(const std::string& v) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
{
	push_back(v.data(), v.size());
}

template<typename PodType, std::size_t N>
inline Buffer::Buffer(PodType (&value)[N]) :
	data_(const_cast<char *>(value)), size_(N - 1), capacity_(N - 1), edit_mode_(EDIT_NOTHING)
{
}

inline Buffer::Buffer(const Buffer& v) :
	data_(0), size_(0), capacity_(0), edit_mode_(EDIT_ALL)
{
	push_back(v.data(), v.size());
}

inline Buffer::Buffer(Buffer&& v) :
	data_(v.data_),
	size_(v.size_),
	capacity_(v.capacity_),
	edit_mode_(v.edit_mode_)
{

	v.data_ = 0;
	v.size_ = 0;
	v.capacity_ = 0;
	v.edit_mode_ = EDIT_ALL;
}

inline Buffer& Buffer::operator=(Buffer&& v)
{
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

inline Buffer& Buffer::operator=(const Buffer& v)
{
	clear();
	push_back(v.data(), v.size());

	return *this;
}

inline Buffer& Buffer::operator=(const std::string& v)
{
	clear();
	push_back(v.data(), v.size());

	return *this;
}

inline Buffer& Buffer::operator=(const value_type *v)
{
	clear();
	push_back(v, std::strlen(v));

	return *this;
}

inline Buffer::~Buffer()
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

inline void Buffer::assertMutable()
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

inline const Buffer::value_type *Buffer::data() const
{
	return data_;
}

inline bool Buffer::empty() const
{
	return !size_;
}

inline const Buffer::value_type *Buffer::c_str() const
{
	const_cast<Buffer *>(this)->reserve(size_ + 1);
	const_cast<Buffer *>(this)->data_[size_] = '\0';

	return data_;
}

inline std::size_t Buffer::size() const
{
	return size_;
}

inline void Buffer::resize(std::size_t value)
{
	if (value > capacity_)
		reserve(value);

	size_ = value;
}

inline std::size_t Buffer::capacity() const
{
	return capacity_;
}

inline void Buffer::capacity(std::size_t value)
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

inline void Buffer::reserve(std::size_t value)
{
	if (value > capacity_)
	{
		capacity(value + CHUNK_SIZE - (value % CHUNK_SIZE));
	}
}

inline void Buffer::clear()
{
	resize(0);
}

inline Buffer::operator helper_type() const
{
	return !empty() ? &helper::i : 0;
}

inline bool Buffer::operator!() const
{
	return empty();
}

inline Buffer::iterator Buffer::begin() const
{
	return data_;
}

inline Buffer::iterator Buffer::end() const
{
	return data_ + size_;
}

inline Buffer::const_iterator Buffer::cbegin() const
{
	return data_;
}

inline Buffer::const_iterator Buffer::cend() const
{
	return data_ + size_;
}

inline void Buffer::push_back(value_type value)
{
	reserve(size_ + sizeof(value));

	data_[size_++] = value;
}

inline void Buffer::push_back(int value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%d", value);
	push_back(buf, n);
}

inline void Buffer::push_back(const value_type *value)
{
	if (std::size_t len = std::strlen(value))
	{
		reserve(size_ + len);

		std::memcpy(end(), value, len);
		size_ += len;
	}
}

inline void Buffer::push_back(const Buffer& value)
{
	if (std::size_t len = value.size())
	{
		reserve(size_ + len);

		std::memcpy(end(), value.begin(), len);
		size_ += len;
	}
}

inline void Buffer::push_back(const BufferRef& value)
{
	if (std::size_t len = value.size())
	{
		reserve(size_ + len);

		std::memcpy(end(), value.begin(), len);
		size_ += len;
	}
}

inline void Buffer::push_back(const std::string& value)
{
	if (std::size_t len = value.size())
	{
		reserve(size_ + len);

		std::memcpy(end(), value.data(), len);
		size_ += len;
	}
}

inline void Buffer::push_back(const void *value, std::size_t size)
{
	if (size)
	{
		reserve(size_ + size);

		std::memcpy(end(), value, size);
		size_ += size;
	}
}

template<typename PodType, std::size_t N>
inline void Buffer::push_back(PodType (&value)[N])
{
	push_back(reinterpret_cast<const void *>(value), N - 1);
}

inline Buffer::value_type& Buffer::operator[](std::size_t index)
{
	assert(index >= 0);
	assert(index < size_);

	return data_[index];
}

inline const Buffer::value_type& Buffer::operator[](std::size_t index) const
{
	assert(index >= 0 && index < size_);

	return data_[index];
}

inline std::string Buffer::str() const
{
	return std::string(data_, data_ + size_);
}

inline BufferRef Buffer::ref(std::size_t offset) const
{
	assert(offset <= size_);

	return BufferRef(this, offset, size_ - offset);
}

inline BufferRef Buffer::ref(std::size_t offset, std::size_t count) const
{
	assert(offset >= 0);
	assert(offset + count <= size_);

	return BufferRef(this, offset, count);
}

inline BufferRef Buffer::operator()(std::size_t offset) const
{
	assert(offset >= 0);
	assert(offset <= size_);

	return BufferRef(this, offset, size_ - offset);
}

inline BufferRef Buffer::operator()(std::size_t offset, std::size_t count) const
{
	assert(offset >= 0);
	assert(offset + count < size_);

	return BufferRef(this, offset, count);
}

inline std::string Buffer::substr(std::size_t offset) const
{
	return std::string(data_ + offset, std::min(offset, size_));
}

inline std::string Buffer::substr(std::size_t offset, std::size_t count) const
{
	return std::string(data_ + offset, data_ + std::min(offset + count, size_));
}

inline Buffer Buffer::from_copy(const value_type *data, std::size_t count)
{
	Buffer result(count);
	result.push_back(data);
	return result;
}
// }}}

// {{{ ConstBuffer impl
template<typename PodType, std::size_t N>
inline ConstBuffer::ConstBuffer(PodType (&value)[N]) :
	Buffer(value)
{
}

inline ConstBuffer::ConstBuffer(const value_type *value, std::size_t n) :
	Buffer(value, n)
{
}
// }}}

// {{{ FixedBuffer impl
template<std::size_t N>
inline FixedBuffer<N>::FixedBuffer() :
	Buffer()
{
	data_ = fixed_;
	size_ = 0;
	capacity_ = N;
	edit_mode_ = EDIT_NO_RESIZE;
}
// }}}

// {{{ free function impl
inline bool equals(const Buffer& a, const Buffer& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool equals(const Buffer& a, const char *b)
{
	std::size_t bsize = std::strlen(b);

	return a.size() == bsize
		&& (a.data() == b || std::memcmp(a.data(), b, bsize) == 0);
}

template<typename PodType, std::size_t N>
bool equals(const Buffer& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	if (a.data() == b)
		return true;

	return std::memcmp(a.data(), b, bsize) == 0;
}

// --------------------------------------------------------------------
inline bool iequals(const Buffer& a, const Buffer& b)
{
	if (&a == &b)
		return true;

	if (a.size() != b.size())
		return false;

	return a.data() == b.data() || strncasecmp(a.data(), b.data(), a.size()) == 0;
}

inline bool iequals(const Buffer& a, const char *b)
{
	std::size_t bsize = b ? std::strlen(b) : 0;

	if (a.size() != bsize)
		return false;

	if (strncasecmp(a.data(), b, bsize) != 0)
		return false;

	return true;
}

template<typename PodType, std::size_t N>
bool iequals(const Buffer& a, PodType (&b)[N])
{
	const std::size_t bsize = N - 1;

	if (a.size() != bsize)
		return false;

	if (strncasecmp(a.data(), b, bsize) != 0)
		return false;

	return true;
}

// ------------------------------------------------------------------------
inline bool operator==(const x0::Buffer& a, const x0::Buffer& b)
{
	return equals(a, b);
}

inline bool operator==(const x0::Buffer& a, const char *b)
{
	return equals(a, b);
}

template<typename PodType, std::size_t N> bool operator==(const Buffer& a, PodType (&b)[N])
{
	return equals<PodType, N>(a, b);
}
// }}}

} // namespace x0

// }}}

#endif
