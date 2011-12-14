/* <x0/Buffer.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_buffer_hpp
#define sw_x0_buffer_hpp (1)

#include <x0/Api.h>
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
class X0_API Buffer
{
public:
	typedef char value_type;
	typedef value_type * iterator;
	typedef const value_type * const_iterator;

	//static const std::size_t CHUNK_SIZE = 4096;
	enum { CHUNK_SIZE = 4096 };

	struct helper { int i; };
	typedef int (helper::*helper_type);

	struct Hashable {
		enum {
			bucket_size = 4, // 0 < bucket_size
			min_buckets = 8  // min_buckets = 2 ^^ N, 0 < N
		};

		size_t operator()(const Buffer& buf) const;
		bool operator()(const Buffer& a, const Buffer& b) const;
	};

private:
	friend class BufferRef;

protected:
	value_type *data_;
	std::size_t size_;
	std::size_t capacity_;

public:
	Buffer();
	explicit Buffer(std::size_t _capacity);
	Buffer(const value_type *_data, std::size_t _size); // XXX better be private?
	explicit Buffer(const BufferRef& v);
	Buffer(const char* value);
	explicit Buffer(const std::string& v);
	template<typename PodType, std::size_t N> explicit Buffer(PodType (&value)[N]);
	Buffer(const Buffer& v);
	Buffer(Buffer&& v);
	Buffer& operator=(Buffer&& v);
	Buffer& operator=(const Buffer& v);
	Buffer& operator=(const std::string& v);
	Buffer& operator=(const value_type *v);
	virtual ~Buffer();

	void swap(Buffer& other);

	// attributes
	const value_type *data() const;

	bool empty() const;
	std::size_t size() const;
	bool resize(std::size_t value);

	std::size_t capacity() const;
	virtual bool setCapacity(std::size_t value);

	bool reserve(std::size_t value);
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
	void push_back(long value);
	void push_back(long long value);
	void push_back(unsigned value);
	void push_back(unsigned long value);
	void push_back(unsigned long long value);
	void push_back(const value_type *value);
	void push_back(const Buffer& value);
	void push_back(const BufferRef& value);
	void push_back(const BufferRef& value, size_t offset, size_t length);
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
	static Buffer fromCopy(const value_type *data, std::size_t count);
	static void dump(const void *bytes, std::size_t length, const char *description = nullptr);

	void dump(const char *description = nullptr);
};

// free functions
X0_API bool equals(const Buffer& a, const Buffer& b);
X0_API bool equals(const Buffer& a, const char *b);
template<typename PodType, std::size_t N> X0_API bool equals(const Buffer& a, PodType (&b)[N]);

X0_API bool iequals(const Buffer& a, const Buffer& b);
X0_API bool iequals(const Buffer& a, const char *b);
template<typename PodType, std::size_t N> X0_API bool iequals(const Buffer& a, PodType (&b)[N]);

X0_API bool operator==(const Buffer& a, const Buffer& b);
X0_API bool operator==(const Buffer& a, const char *b);
template<typename PodType, std::size_t N> X0_API bool operator==(const Buffer& a, PodType (&b)[N]);

X0_API Buffer& operator<<(Buffer& b, Buffer::value_type v);
X0_API Buffer& operator<<(Buffer& b, int v);
X0_API Buffer& operator<<(Buffer& b, long v);
X0_API Buffer& operator<<(Buffer& b, long long v);
X0_API Buffer& operator<<(Buffer& b, unsigned v);
X0_API Buffer& operator<<(Buffer& b, unsigned long v);
X0_API Buffer& operator<<(Buffer& b, unsigned long long v);
X0_API Buffer& operator<<(Buffer& b, const Buffer::value_type *v);
X0_API Buffer& operator<<(Buffer& b, const Buffer& v);
X0_API Buffer& operator<<(Buffer& b, const BufferRef& v);
X0_API Buffer& operator<<(Buffer& b, const std::string& v);
template<typename PodType, std::size_t N> X0_API Buffer& operator<<(Buffer& b, PodType (&v)[N]);

} // namespace x0

//@}

#include <x0/BufferRef.h>

// {{{ impl

namespace x0 {

// {{{ Buffer::Hashable impl
inline size_t Buffer::Hashable::operator()(const Buffer& buf) const
{
	return reinterpret_cast<size_t>(&buf);
}

inline bool Buffer::Hashable::operator()(const Buffer& a, const Buffer& b) const
{
	return equals(a, b);
}
// }}}

// {{{ Buffer impl
inline Buffer::Buffer() :
	data_(0), size_(0), capacity_(0)
{
}

inline Buffer::Buffer(std::size_t _capacity) :
	data_(0), size_(0), capacity_(0)
{
	setCapacity(_capacity);
}

inline Buffer::Buffer(const value_type *_data, std::size_t _size) :
	data_(const_cast<value_type *>(_data)), size_(_size), capacity_(_size)
{
}

inline Buffer::Buffer(const BufferRef& v) :
	data_(0), size_(0), capacity_(0)
{
	push_back(v.data(), v.size());
}

inline Buffer::Buffer(const char* v) :
	data_(0), size_(0), capacity_(0)
{
	std::size_t len = std::strlen(v);
	setCapacity(len + 1);
	push_back(v);
}

inline Buffer::Buffer(const std::string& v) :
	data_(0), size_(0), capacity_(0)
{
	push_back(v.c_str(), v.size() + 1);
	resize(v.size());
}

template<typename PodType, std::size_t N>
inline Buffer::Buffer(PodType (&value)[N]) :
	data_(0), size_(0), capacity_(N - 1)
{
	push_back(value, N);
	resize(N - 1);
}

inline Buffer::Buffer(const Buffer& v) :
	data_(0), size_(0), capacity_(0)
{
	push_back(v.data(), v.size());
}

inline Buffer::Buffer(Buffer&& v) :
	data_(v.data_),
	size_(v.size_),
	capacity_(v.capacity_)
{
	v.data_ = 0;
	v.size_ = 0;
	v.capacity_ = 0;
}

inline Buffer& Buffer::operator=(Buffer&& v)
{
	if (capacity_)
		setCapacity(0);

	data_ = v.data_;
	size_ = v.size_;
	capacity_ = v.capacity_;

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
	setCapacity(0);
}

inline void Buffer::swap(Buffer& other)
{
	std::swap(data_, other.data_);
	std::swap(size_, other.size_);
	std::swap(capacity_, other.capacity_);
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
	if (const_cast<Buffer *>(this)->reserve(size_ + 1))
		const_cast<Buffer *>(this)->data_[size_] = '\0';

	return data_;
}

inline std::size_t Buffer::size() const
{
	return size_;
}

inline bool Buffer::resize(std::size_t value)
{
	if (!reserve(value))
		return false;

	size_ = value;
	return true;
}

inline std::size_t Buffer::capacity() const
{
	return capacity_;
}

inline bool Buffer::reserve(std::size_t value)
{
	if (value <= capacity_)
		return true;

	return setCapacity(value);
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
	if (reserve(size_ + sizeof(value))) {
		data_[size_++] = value;
	}
}

inline void Buffer::push_back(int value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%d", value);
	push_back(buf, n);
}

inline void Buffer::push_back(long value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%ld", value);
	push_back(buf, n);
}

inline void Buffer::push_back(long long value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%lld", value);
	push_back(buf, n);
}

inline void Buffer::push_back(unsigned value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%u", value);
	push_back(buf, n);
}

inline void Buffer::push_back(unsigned long value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%lu", value);
	push_back(buf, n);
}

inline void Buffer::push_back(unsigned long long value)
{
	char buf[32];
	int n = std::snprintf(buf, sizeof(buf), "%llu", value);
	push_back(buf, n);
}

inline void Buffer::push_back(const value_type *value)
{
	if (std::size_t len = std::strlen(value)) {
		if (reserve(size_ + len)) {
			std::memcpy(end(), value, len);
			size_ += len;
		}
	}
}

inline void Buffer::push_back(const Buffer& value)
{
	if (std::size_t len = value.size()) {
		if (reserve(size_ + len)) {
			std::memcpy(end(), value.begin(), len);
			size_ += len;
		}
	}
}

inline void Buffer::push_back(const BufferRef& value)
{
	if (std::size_t len = value.size()) {
		if (reserve(size_ + len)) {
			std::memcpy(end(), value.begin(), len);
			size_ += len;
		}
	}
}

inline void Buffer::push_back(const BufferRef& value, size_t offset, size_t length)
{
	assert(value.size() <= offset + length);

	if (!length)
		return;

	if (reserve(size_ + length)) {
		memcpy(end(), value.begin() + offset, length);
		size_ += length;
	}
}

inline void Buffer::push_back(const std::string& value)
{
	if (std::size_t len = value.size()) {
		if (reserve(size_ + len)) {
			std::memcpy(end(), value.data(), len);
			size_ += len;
		}
	}
}

inline void Buffer::push_back(const void *value, std::size_t size)
{
	if (size) {
		if (reserve(size_ + size)) {
			std::memcpy(end(), value, size);
			size_ += size;
		}
	}
}

template<typename PodType, std::size_t N>
inline void Buffer::push_back(PodType (&value)[N])
{
	push_back(reinterpret_cast<const void *>(value), N - 1);
}

inline Buffer::value_type& Buffer::operator[](std::size_t index)
{
	assert(index < size_);

	return data_[index];
}

inline const Buffer::value_type& Buffer::operator[](std::size_t index) const
{
	assert(index < size_);

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
	assert(offset + count <= size_);

	return BufferRef(this, offset, count);
}

inline BufferRef Buffer::operator()(std::size_t offset) const
{
	assert(offset <= size_);

	return BufferRef(this, offset, size_ - offset);
}

inline BufferRef Buffer::operator()(std::size_t offset, std::size_t count) const
{
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

inline Buffer Buffer::fromCopy(const value_type *data, std::size_t count)
{
	Buffer result(count);
	result.push_back(data, count);
	return result;
}

inline void Buffer::dump(const char *description)
{
	dump(data_, size_, description);
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

inline Buffer& operator<<(Buffer& b, Buffer::value_type v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, int v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, long v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, long long v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, unsigned v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, unsigned long v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, unsigned long long v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, const Buffer::value_type *v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, const Buffer& v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, const BufferRef& v)
{
	b.push_back(v);
	return b;
}

inline Buffer& operator<<(Buffer& b, const std::string& v)
{
	b.push_back(v);
	return b;
}

template<typename PodType, std::size_t N> inline Buffer& operator<<(Buffer& b, PodType (&v)[N])
{
	b.push_back<PodType, N>(v);
	return b;
}

// }}}

} // namespace x0

// }}}

#endif
