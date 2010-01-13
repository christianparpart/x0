/* <x0/composite_buffer.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */
#ifndef x0_composite_buffer_hpp
#define x0_composite_buffer_hpp

#include <x0/property.hpp>
#include <x0/buffer.hpp>
#include <x0/api.hpp>

#include <string>
#include <utility>
#include <sys/types.h>
#include <sys/sendfile.h>	// sendfile()
#include <sys/socket.h>		// sendto()
#include <unistd.h>			// close(), write(), sysconf()

#include <asio.hpp>
#include <iostream> // clog

// XXX favor buffer_chunk over iovec_chunk, and see how it performs better in speed than iovec_chunk.
#define COMPOSITE_BUFFER_NO_IOVEC 1

namespace x0 {

//! \addtogroup base
//@{

/**
 * \brief Class for constructing and sending up to complex composite buffers from various sources.
 *
 * A composite buffer - once fully created - is meant to be sent only <b>once</b>.
 * This class shall support asynchronous I/O file.
 *
 * \code
 *	template<class Target, class CompletionHandler>
 *	void generate(Target& target, const CompletionHandler& handler)
 *	{
 * 		x0::composite_buffer cb;
 *
 *		cb.push_back("Hello, World");
 *		cb.push_back('\n');
 *
 * 		struct stat st;
 * 		if (stat("somefile.txt", &st) != -1)
 * 		{
 * 			int fd = open("somefile.txt", O_RDONLY);
 * 			if (fd != -1)
 * 			{
 * 				cb.push_back(fd, 0, st.st_size, true);
 * 			}
 * 		}
 *
 * 		x0::async_write(target, cb, handler);
 * 	}
 *
 * 	void completion_handler(boost::system::error_code ec, std::size_t bytes_transferred)
 * 	{
 * 		if (!ec)
 * 		{
 * 			std::cerr << "write operation completed." << std::endl;
 * 		}
 * 		else
 * 		{
 * 			std::cerr << "error: " << ec.message() << std::endl;
 * 		}
 * 	}
 * \endcode
 */
class composite_buffer
{
public:
	struct chunk;
	struct buffer_chunk;
	struct iovec_chunk;
	struct fd_chunk;

	class iterator;

	class write_visitor;

private:
	template<class Socket, class CompletionHandler> class write_handler;

private:
	/** ensures that the tail of our chunk queue is of type T. */
	template<typename T> void ensure_tail();

	chunk *front_;		//!< chunk at the front/beginning of the buffer
	chunk *back_;		//!< chunk at the back/end of the buffer
	std::size_t size_;	//!< total size in bytes of this buffer (sum of bytes of all buffer chunks)

public:
	/** initializes a composite buffer. */
	composite_buffer();

	/** initializes this composite buffer with chunks from  \p v by taking over ownership of \p v's chunks.
	 *
	 * \param v the other composite buffer to take over the chunks from.
	 *
	 * \note After this initialization, the other composite buffer \p v does not contain any chunks anymore.
	 */
	composite_buffer(composite_buffer&& v);
	composite_buffer(const composite_buffer&);
	composite_buffer& operator=(const composite_buffer&) = delete;

	~composite_buffer();

	void swap(composite_buffer&& v);

	/** assigns this composite buffer with chunks from  \p v by taking over ownership of \p v's chunks.
	 *
	 * \param v the other composite buffer to take over the chunks from.
	 *
	 * Current chunks within this composite buffer will be freed.
	 *
	 * \note After this initialization, the other composite buffer \p v does not contain any chunks anymore.
	 */
	composite_buffer& operator=(composite_buffer&& v);

	/** iterator, pointing to the front_ chunk. */
	iterator begin() const;

	/** iterator, representing the end of chunk list. */
	iterator end() const;

	/** removes first chunk in list if available. */
	void remove_front();

	/** retrieves first chunk in list. */
	chunk *front() const;

	/** retrieves last chunk in list. */
	chunk *back() const;

	/** retrieves the total number of bytes of all chunks. */
	std::size_t size() const;

	/** checks wether this composite buffer is empty or not. */
	bool empty() const;

	/** appends a character value. */
	void push_back(char value);

	/** appends a C-String value. */
	void push_back(const char *value);

	/** appends a string value. */
	void push_back(const std::string& value);

	/** appends a const buffer.
	 *
	 * \param buffer the const buffer
	 * \param size how many bytes of this const buffer to use.
	 *
	 * This buffer may <b>NOT</b> be destructed until used by this object, otherwise the sendto()
	 * may result into undefined behaviour.
	 */
	void push_back(const void *buffer, int size);

	/** appends a chunk that will read from given file descriptor on sendto().
	 *
	 * \param fd the file descriptor to read from on sendto()
	 * \param offset the offset inside the input file.
	 * \param size how many bytes to read from input and to pass to destination file.
	 * \param close indicates wether given file descriptor should be closed on chunk destruction.
	 *
	 * On sendto() this chunk may be optimized to directly pass the input buffers to the output buffers
	 * of their corresponding file descriptors by using sendfile() kernel system call.
	 */
	void push_back(int fd, off_t offset, std::size_t size, bool close);

	/** appends another composite buffer to this buffer.
	 *
	 * \param source the source buffer to append to this buffer.
	 *
	 * As performance matters, the source buffer will be directly integrated into the destination buffer,
	 * thus source.size() will be 0 after this operation.
	 */
	void push_back(composite_buffer& source);

	/** appends a podtype value at the back of the chunk chain.
	 * \param data the data to be append.
	 */
	template<typename PodType, std::size_t N>
	void push_back(PodType (&data)[N]);
};

/** chunk base class.
 * \see string_chunk, iovec_chunk, fd_chunk
 */
struct composite_buffer::chunk
{
public:
	enum chunk_type { cbuffer, ciov, cfd };

protected:
	/** initializes chunk base.
	 *
	 * \param ct chunk type, assigned to \p type property.
	 * \param sz chunk size in bytes, assigned to \p size property.
	 */
	explicit chunk(chunk_type ct);

public:
	virtual ~chunk();

	/** chunk type. */
	chunk_type type() const;

	/** chunk size in bytes. */
	virtual std::size_t size() const = 0;

	/** next chunk in buffer. */
	chunk *next() const;
	void next(chunk *);

	/**
	 * invokes <code>return v.write(*this);</code> to let write_visitor \p v visit this chunk.
	 *
	 * \param v write_visitor to let visit us.
	 */
	virtual ssize_t accept(write_visitor& v) const = 0;

private:
	chunk_type type_;
	chunk *next_;
};

struct composite_buffer::buffer_chunk :
	public chunk
{
private:
	x0::buffer buffer_;

public:
	enum { type_val = cbuffer };

public:
	buffer_chunk();

	x0::buffer& buffer();
	const x0::buffer& buffer() const;

	virtual std::size_t size() const;
	virtual ssize_t accept(write_visitor& v) const;
};

/** iovec chunk.
 * \see composite_buffer::chunk, composite_buffer::fd_chunk
 */
struct composite_buffer::iovec_chunk :
	public chunk
{
public:
	typedef std::vector<iovec> vector;
	typedef vector::iterator iterator;
	typedef vector::const_iterator const_iterator;

	enum { type_val = ciov };

private:
	vector vec_;
	std::size_t size_;
	std::size_t veclimit_;
	buffer buffer_;

public:
	iovec_chunk();

	/** pushes a single character value at the end of the iovec vector.
	 */
	void push_back(char value);

	/** pushes a string value at the end of the iovec vector.
	 */
	void push_back(const std::string& value);

	/** pushes a const buffer \p p of size \p n at the end of the iovec vector.
	 */
	void push_back(const void *p, std::size_t n);

	/** retrieves a reference to the internal iovec vector. */
	const std::vector<iovec>& value() const;

	/** retrieves the number of iovec-elements within this iovec_chunk. */
	std::size_t length() const;

	/** retrieves the iovec vector at given \p index. */
	const iovec& operator[](std::size_t index) const;

	/** retrieves an iterator to the beginning of the iovec vector. */
	iterator begin();

	/** retrieves an iterator to the end of the iovec vector. */
	iterator end();

	/** retrieves a const iterator to the beginning of the iovec vector. */
	const_iterator cbegin() const;

	/** retrieves a const iterator to the end of the iovec vector. */
	const_iterator cend() const;

	virtual std::size_t size() const;
	virtual ssize_t accept(write_visitor& v) const;
};

/** fd chunk.
 *
 * \note in order to have sendfile(2) to function, the input file descriptor <b>must</b> be mmap()'able,
 * and the output file descriptor <b>must</b> be of type socket.
 */
struct composite_buffer::fd_chunk :
	public chunk
{
private:
	// TODO: code cleanup! (fd_ vs fd, offset_ vs offset, etc etc.)
	int fd_;
	off_t offset_;
	std::size_t size_;
	bool close_;

public:
	enum { type_val = cfd };

public:
	fd_chunk(int _fd, off_t _offset, std::size_t _size, bool _close);
	~fd_chunk();

	/** holds the file descriptor to this chunk represents its data for. */
	value_property<int> fd;

	/** holds the offset inside the data of the file descriptor to start reading at. */
	value_property<off_t> offset;

	/** boolean value, determining wether or not to close the file descriptor when this chunk is to be destructed. */
	value_property<bool> close;

	virtual std::size_t size() const;
	virtual ssize_t accept(write_visitor& v) const;
};

/**
 * chunk write_visitor interface, used to <b>write</b> chunks into some destination.
 */
class composite_buffer::write_visitor
{
public:
	virtual ssize_t write(const buffer_chunk&) = 0;//!< visits a buffer_chunk object.
	virtual ssize_t write(const iovec_chunk&) = 0;	//!< visits an iovec_chunk object.
	virtual ssize_t write(const fd_chunk&) = 0;	//!< visits an fd_chunk object.
};

// {{{ chunk impl
inline composite_buffer::chunk::chunk(chunk_type ct) :
	type_(ct), next_(0)
{
}

inline composite_buffer::chunk::~chunk()
{
	delete next_;
}

inline composite_buffer::chunk::chunk_type composite_buffer::chunk::type() const
{
	return type_;
}

inline composite_buffer::chunk *composite_buffer::chunk::next() const
{
	return next_;
}

inline void composite_buffer::chunk::next(chunk *value)
{
	next_ = value;
}
// }}}

// {{{ buffer_chunk impl
inline composite_buffer::buffer_chunk::buffer_chunk() :
	chunk(cbuffer),
	buffer_()
{
}

inline x0::buffer& composite_buffer::buffer_chunk::buffer()
{
	return buffer_;
}

inline const x0::buffer& composite_buffer::buffer_chunk::buffer() const
{
	return buffer_;
}

inline std::size_t composite_buffer::buffer_chunk::size() const
{
	return buffer_.size();
}

inline ssize_t composite_buffer::buffer_chunk::accept(write_visitor& v) const
{
	return v.write(*this);
}
// }}}

// {{{ iovec_chunk impl
inline composite_buffer::iovec_chunk::iovec_chunk() :
	chunk(ciov), vec_(), size_(0), veclimit_(sysconf(_SC_IOV_MAX)), buffer_(buffer::CHUNK_SIZE)
{
}

inline void composite_buffer::iovec_chunk::push_back(char value)
{
	buffer_.push_back(value);
	push_back(buffer_.end() - sizeof(char), sizeof(char));
}

inline void composite_buffer::iovec_chunk::push_back(const std::string& value)
{
	if (!value.empty())
	{
		buffer_.push_back(value);

		push_back(buffer_.end() - value.size(), value.size());
	}
}

inline void composite_buffer::iovec_chunk::push_back(const void *p, std::size_t n)
{
	iovec iov;

	iov.iov_base = const_cast<void *>(p);
	iov.iov_len = n;

	vec_.push_back(iov);
	size_ += n;
}

inline const std::vector<iovec>& composite_buffer::iovec_chunk::value() const
{
	return vec_;
}

inline std::size_t composite_buffer::iovec_chunk::length() const
{
	return vec_.size();
}

inline const iovec& composite_buffer::iovec_chunk::operator[](std::size_t index) const
{
	return vec_[index];
}

inline composite_buffer::iovec_chunk::iterator composite_buffer::iovec_chunk::begin()
{
	return vec_.begin();
}

inline composite_buffer::iovec_chunk::iterator composite_buffer::iovec_chunk::end()
{
	return vec_.end();
}

inline composite_buffer::iovec_chunk::const_iterator composite_buffer::iovec_chunk::cbegin() const
{
	return vec_.cbegin();
}

inline composite_buffer::iovec_chunk::const_iterator composite_buffer::iovec_chunk::cend() const
{
	return vec_.cend();
}

inline std::size_t composite_buffer::iovec_chunk::size() const
{
	return size_;
}

inline ssize_t composite_buffer::iovec_chunk::accept(write_visitor& v) const
{
	return v.write(*this);
}
// }}}

// {{{ fd_chunk impl
inline composite_buffer::fd_chunk::fd_chunk(int _fd, off_t _offset, std::size_t _size, bool _close) :
	chunk(cfd), size_(_size), fd(_fd), offset(_offset), close(_close)
{
}

inline composite_buffer::fd_chunk::~fd_chunk()
{
	if (close)
	{
		::close(fd);
	}
}

inline std::size_t composite_buffer::fd_chunk::size() const
{
	return size_;
}

inline ssize_t composite_buffer::fd_chunk::accept(write_visitor& v) const
{
	return v.write(*this);
}
// }}}

// {{{ composite_buffer::iterator
/** composite_buffer's standard chunk iteration class.
 */
class composite_buffer::iterator
{
private:
	const chunk *current_;

public:
	iterator(const chunk *c) :
		current_(c)
	{
	}

	iterator& operator++()
	{
		if (current_)
		{
			current_ = current_->next();
		}

		return *this;
	}

	const chunk *operator->() const
	{
		return current_;
	}

	const chunk& operator*() const
	{
		return *current_;
	}

	friend bool operator!=(const iterator& a, const iterator& b)
	{
		return a.current_ != b.current_;
	}

	friend bool operator==(const iterator& a, const iterator& b)
	{
		return a.current_ == b.current_;
	}
}; // }}}

// {{{ composite_buffer impl
inline composite_buffer::composite_buffer() :
	front_(0), back_(0), size_(0)
{
}

inline composite_buffer::composite_buffer(composite_buffer&& v) :
	front_(std::move(v.front_)),
	back_(std::move(v.back_)),
	size_(std::move(v.size_))
{
}

inline composite_buffer::composite_buffer(const composite_buffer& v) :
	front_(v.front_),
	back_(v.back_),
	size_(v.size_)
{
	const_cast<composite_buffer *>(&v)->front_ = 0;
	const_cast<composite_buffer *>(&v)->back_ = 0;
	const_cast<composite_buffer *>(&v)->size_ = 0;
}

inline composite_buffer& composite_buffer::operator=(composite_buffer&& v)
{
	front_ = std::move(v.front_);
	back_ = std::move(v.back_);
	size_ = std::move(v.size_);

	return *this;
}

inline void composite_buffer::swap(composite_buffer&& v)
{
	using std::swap;

	swap(front_, v.front_);
	swap(back_, v.back_);
	swap(size_, v.size_);
}

inline composite_buffer::~composite_buffer()
{
	delete front_;
}

inline composite_buffer::iterator composite_buffer::begin() const
{
	return iterator(front_);
}

inline composite_buffer::iterator composite_buffer::end() const
{
	return iterator(0);
}

inline void composite_buffer::remove_front()
{
	if (front_)
	{
		chunk *new_front = front_->next();

		size_ -= front_->size();
		front_->next(0);
		delete front_;

		front_ = new_front;
	}
}

inline composite_buffer::chunk *composite_buffer::front() const
{
	return front_;
}

inline composite_buffer::chunk *composite_buffer::back() const
{
	return back_;
}

inline std::size_t composite_buffer::size() const
{
	return size_;
}

inline bool composite_buffer::empty() const
{
	return !size_;
}

inline void composite_buffer::push_back(char value)
{
#if defined(COMPOSITE_BUFFER_NO_IOVEC) && COMPOSITE_BUFFER_NO_IOVEC
	ensure_tail<buffer_chunk>();
	static_cast<buffer_chunk *>(back_)->buffer().push_back(value);
#else
	ensure_tail<iovec_chunk>();
	static_cast<iovec_chunk *>(back_)->push_back(value);
#endif

	size_ += sizeof(value);
}

inline void composite_buffer::push_back(const char *value)
{
	push_back(value, std::strlen(value));
}

inline void composite_buffer::push_back(const std::string& value)
{
#if defined(COMPOSITE_BUFFER_NO_IOVEC) && COMPOSITE_BUFFER_NO_IOVEC
	ensure_tail<buffer_chunk>();
	static_cast<buffer_chunk *>(back_)->buffer().push_back(value);
#else
	ensure_tail<iovec_chunk>();
	static_cast<iovec_chunk *>(back_)->push_back(value);
#endif

	size_ += value.size();
}

inline void composite_buffer::push_back(const void *buffer, int size)
{
#if defined(COMPOSITE_BUFFER_NO_IOVEC) && COMPOSITE_BUFFER_NO_IOVEC
	ensure_tail<buffer_chunk>();
	static_cast<buffer_chunk *>(back_)->buffer().push_back(buffer, size);
#else
	ensure_tail<iovec_chunk>();
	static_cast<iovec_chunk *>(back_)->push_back(buffer, size);
#endif

	size_ += size;
}

template<typename PodType, std::size_t N>
inline void composite_buffer::push_back(PodType (&data)[N])
{
	push_back(static_cast<const void *>(data), sizeof(PodType) * (N - 1));
}

inline void composite_buffer::push_back(int fd, off_t offset, size_t size, bool close)
{
	if (back_)
	{
		back_->next(new fd_chunk(fd, offset, size, close));
		back_ = back_->next();
	}
	else
	{
		front_ = back_ = new fd_chunk(fd, offset, size, close);
	}

	size_ += back_->size();
}

inline void composite_buffer::push_back(composite_buffer& buffer)
{
	if (back_)
	{
		back_->next(buffer.front_);
		back_ = buffer.back_;
		size_ += buffer.size_;
	}
	else
	{
		front_ = buffer.front_;
		back_ = buffer.back_;
		size_ = buffer.size_;
	}

	buffer.front_ = buffer.back_ = 0;
	buffer.size_ = 0;
}

/** ensures tail chunk node is of specific type \p T by possibly appending a new chunk node to the list of chunk.
 */
template<typename T>
inline void composite_buffer::ensure_tail()
{
	if (!back_)
	{
		front_ = back_ = new T();
	}
	else if (back_->type() != static_cast<chunk::chunk_type>(T::type_val))
	{
		back_->next(new T());
		back_ = back_->next();
	}
}
// }}}

//@}

} // namespace x0

#endif
