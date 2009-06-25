#ifndef x0_composite_buffer_hpp
#define x0_composite_buffer_hpp

#include <string>
#include <sys/types.h>
#include <sys/sendfile.h>	// sendfile()
#include <sys/socket.h>		// sendto()
#include <sys/stat.h>		// stat()
#include <unistd.h>			// close(), write(), sysconf()

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>

namespace x0 {

/**
 * \ingroup common
 * \brief Class for constructing and sending up to complex composite buffers from various sources.
 *
 * A composite buffer - once fully created - is meant to be sent only <b>once</b>.
 * This class shall support asynchronous I/O file.
 *
 * \code
 *	template<class Socket, class CompletionHandler>
 *	void generate(Socket& socket, const CompletionHandler& handler)
 *	{
 * 		composite_buffer cb;
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
 * 		cb.async_write(composite_buffer::socket_writer<Socket>(socket), handler);
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
 * 			std::cerr << "error: " << strerror(errno) << std::endl;
 * 		}
 * 	}
 * \endcode
 */
class composite_buffer
{
public:
	struct chunk;
	struct iovec_chunk;
	struct fd_chunk;

	class iterator;

	class visitor;
	template<class Socket> class socket_writer;

	typedef socket_writer<boost::asio::ip::tcp::socket> asio_socket_writer;

private:
	template<class Socket, class CompletionHandler> class write_handler;

private:
	/** ensures that the tail of our chunk queue is of type iovec_chunk. */
	void ensure_iovec_tail();

	/**
	 * write some data into the screen.
	 *
	 * \param socket the socket to write to.
	 * \param handler the completion handler to invoke once current data has been sent 
	 *                and the socket is ready for new data to be transmitted.
	 */
	template<class Writer, class Handler>
	ssize_t async_write_some(Writer writer, const Handler& handler);

	chunk *front_;		//!< chunk at the front/beginning of the buffer
	chunk *back_;		//!< chunk at the back/end of the buffer
	size_t size_;		//!< total size in bytes of this buffer (sum of bytes of all buffer chunks)

public:
	/** initializes a composite buffer. */
	composite_buffer();

	/** initializes this composite buffer with chunks from  \p v by taking over ownership of \p v's chunks.
	 *
	 * \param v the other composite buffer to take over the chunks from.
	 *
	 * \note After this initialization, the other composite buffer \p v does not contain any chunks anymore.
	 */
	composite_buffer(const composite_buffer& v);

	~composite_buffer();

	/** assigns this composite buffer with chunks from  \p v by taking over ownership of \p v's chunks.
	 *
	 * \param v the other composite buffer to take over the chunks from.
	 *
	 * Current chunks within this composite buffer will be freed.
	 *
	 * \note After this initialization, the other composite buffer \p v does not contain any chunks anymore.
	 */
	composite_buffer& operator=(const composite_buffer& v);

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
	size_t size() const;

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
	void push_back(int fd, off_t offset, size_t size, bool close);

	/** appends another composite buffer to this buffer.
	 *
	 * \param source the source buffer to append to this buffer.
	 *
	 * As performance matters, the source buffer will be directly integrated into the destination buffer,
	 * thus source.size() will be 0 after this operation.
	 */
	void push_back(composite_buffer& source);

	template<typename PodType, std::size_t N>
	void push_back(PodType (&data)[N]);

public:
	/** sends this buffer to given destnation.
	 * \param socket the socket to write to.
	 *
	 * Every chunk fully sent to the destination file is automatically removed from this buffer, 
	 * thus subsequent calls to sendto() will continue to sent the remaining chunks, if needed.
	 */
	template<class Writer>
	ssize_t write(Writer writer);

	/** sends this buffer <b>asynchronously</b> to given destnation.
	 * \param socket the socket to write to.
	 * \param handler the completion handler to call once all data has been sent (or an error occured).
	 *
	 * Every chunk fully sent to the destination file is automatically removed from this buffer, 
	 * thus subsequent calls to sendto() will continue to sent the remaining chunks, if needed.
	 */
	template<class Writer, class CompletionHandler>
	void async_write(Writer writer, const CompletionHandler& handler);
};

/** chunk base class.
 * \see string_chunk, iovec_chunk, fd_chunk
 */
struct composite_buffer::chunk
{
	/** chunk type. */
	enum chunk_type { cstring, ciov, cfd } type;

	/** chunk size in bytes. */
	std::size_t size;

	/** next chunk in buffer. */
	chunk *next;

protected:
	/** initializes chunk base.
	 *
	 * \param ct chunk type, assigned to \p type property.
	 * \param sz chunk size in bytes, assigned to \p size property.
	 */
	chunk(chunk_type ct, std::size_t sz);

public:
	virtual ~chunk();

	/**
	 * invokes <code>v.visit(*this);</code> to let visitor \p v visit this chunk.
	 *
	 * \param v visitor to let visit us.
	 */
	virtual void accept(visitor& v) = 0;
};

/** iovec chunk.
 * \see composite_buffer::chunk, composite_buffer::fd_chunk
 */
struct composite_buffer::iovec_chunk :
	public chunk
{
	std::vector<iovec> vec;
	std::size_t veclimit;
	std::vector<std::string> strings;

public:
	iovec_chunk();

	void push_back(char value);
	void push_back(const std::string& value);
	void push_back(const void *p, std::size_t n);

	virtual void accept(visitor& v);
};

/** fd chunk.
 *
 * \note in order to have sendfile(2) to function, the input file descriptor <b>must</b> be mmap()'able,
 * and the output file descriptor <b>must</b> be of type socket.
 */
struct composite_buffer::fd_chunk :
	public chunk
{
	int fd;
	off_t offset;
	bool close;

public:
	fd_chunk(int _fd, off_t _offset, size_t _size, bool _close);
	~fd_chunk();

	virtual void accept(visitor& v);
};

class composite_buffer::visitor
{
public:
	virtual void visit(iovec_chunk&) = 0;
	virtual void visit(fd_chunk&) = 0;
};

template<class Socket>
class composite_buffer::socket_writer : public visitor
{
private:
	Socket& socket_;
	int rv_;

public:
	explicit socket_writer(Socket& socket);

	int write(composite_buffer::chunk& chunk);

	Socket& socket() const;
	int result() const;

	template<class CompletionHandler>
	void callback(const CompletionHandler& handler);

	virtual void visit(iovec_chunk&);
	virtual void visit(fd_chunk&);
};

// {{{ socket_writer impl
template<class Socket>
inline composite_buffer::socket_writer<Socket>::socket_writer(Socket& socket) :
	socket_(socket), rv_()
{
}

template<class Socket>
inline int composite_buffer::socket_writer<Socket>::write(composite_buffer::chunk& chunk)
{
	chunk.accept(*this);
	return rv_;
}

template<class Socket>
inline Socket& composite_buffer::socket_writer<Socket>::socket() const
{
	return socket_;
}

template<class Socket>
inline int composite_buffer::socket_writer<Socket>::result() const
{
	return rv_;
}

template<class Socket>
template<class CompletionHandler>
inline void composite_buffer::socket_writer<Socket>::callback(const CompletionHandler& handler)
{
	socket_.async_write_some(boost::asio::null_buffers(), handler);
}

template<class Socket>
inline void composite_buffer::socket_writer<Socket>::visit(iovec_chunk& chunk)
{
	rv_ = ::writev(socket_.native(), &chunk.vec[0], chunk.vec.size());

	if (rv_ != -1)
	{
		chunk.size -= rv_;
	}

	// TODO error handling if (rv != size), that is, not everything written on O_NONBLOCK
	// (does this even happen?)
}

template<class Socket>
inline void composite_buffer::socket_writer<Socket>::visit(fd_chunk& chunk)
{
	if (chunk.size)
	{
		rv_ = ::sendfile(socket_.native(), chunk.fd, &chunk.offset, chunk.size);

		if (rv_ != -1)
		{
			chunk.size -= rv_;
		}

		// TODO: implement fallback through read()/write()
	}
	else
	{
		rv_ = 0;
	}
}
// }}}

// {{{ chunk impl
inline composite_buffer::chunk::chunk(chunk_type ct, std::size_t sz) :
	type(ct), size(sz), next(0)
{
}

inline composite_buffer::chunk::~chunk()
{
	delete next;
}
// }}}

// {{{ iovec_chunk impl
inline composite_buffer::iovec_chunk::iovec_chunk() :
	chunk(ciov, 0), vec(), veclimit(sysconf(_SC_IOV_MAX)), strings()
{
}

inline void composite_buffer::iovec_chunk::push_back(char value)
{
	push_back(std::string(1, value));
}

inline void composite_buffer::iovec_chunk::push_back(const std::string& value)
{
	strings.push_back(value);
	push_back(strings[strings.size() - 1].data(), value.size());
}

inline void composite_buffer::iovec_chunk::push_back(const void *p, std::size_t n)
{
	iovec iov;

	iov.iov_base = const_cast<void *>(p);
	iov.iov_len = n;

	vec.push_back(iov);
	size += n;
}

inline void composite_buffer::iovec_chunk::accept(visitor& v)
{
	v.visit(*this);
}
// }}}

// {{{ fd_chunk impl
inline composite_buffer::fd_chunk::fd_chunk(int _fd, off_t _offset, size_t _size, bool _close) :
	chunk(cfd, _size), fd(_fd), offset(_offset), close(_close)
{
}

inline composite_buffer::fd_chunk::~fd_chunk()
{
	if (close)
	{
		::close(fd);
	}
}

inline void composite_buffer::fd_chunk::accept(visitor& v)
{
	v.visit(*this);
}
// }}}

// {{{ class composite_buffer::write_handler
template<class Writer, class CompletionHandler>
class composite_buffer::write_handler
{
private:
	composite_buffer& buffer_;
	Writer writer_;
	CompletionHandler handler_;
	std::size_t total_bytes_transferred_;

public:
	write_handler(composite_buffer& buffer, Writer writer, const CompletionHandler& handler)
	  : buffer_(buffer), writer_(writer), handler_(handler)
	{
	}

	void operator()(const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		//total_bytes_transferred_ += bytes_transferred;

		if (!ec && !buffer_.empty())
		{
			ssize_t rv = buffer_.async_write_some(writer_, *this);

			if (rv != -1)
			{
				total_bytes_transferred_ += rv;
			}
		}
		else
		{
			handler_(ec, total_bytes_transferred_);
		}
	}
};
// }}}

// {{{ composite_buffer::iterator
class composite_buffer::iterator
{
private:
	const chunk *current;

public:
	iterator(const chunk *c) :
		current(c)
	{
	}

	iterator& operator++()
	{
		if (current)
		{
			current = current->next;
		}

		return *this;
	}

	const chunk *operator->() const
	{
		return current;
	}

	const chunk& operator*() const
	{
		return *current;
	}

	friend bool operator!=(const iterator& a, const iterator& b)
	{
		return a.current != b.current;
	}
}; // }}}

// {{{ composite_buffer impl
inline composite_buffer::composite_buffer() :
	front_(0), back_(0), size_(0)
{
}

inline composite_buffer::composite_buffer(const composite_buffer& v) :
	front_(v.front_), back_(v.back_), size_(v.size_)
{
	const_cast<composite_buffer *>(&v)->front_ = 0;
	const_cast<composite_buffer *>(&v)->back_ = 0;
	const_cast<composite_buffer *>(&v)->size_ = 0;
}

inline composite_buffer& composite_buffer::operator=(const composite_buffer& v)
{
	front_ = v.front_;
	back_ = v.back_;
	size_ = v.size_;

	const_cast<composite_buffer *>(&v)->front_ = 0;
	const_cast<composite_buffer *>(&v)->back_ = 0;
	const_cast<composite_buffer *>(&v)->size_ = 0;

	return *this;
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
		chunk *new_front = front_->next;

		size_ -= front_->size;
		front_->next = 0;
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

inline size_t composite_buffer::size() const
{
	return size_;
}

inline bool composite_buffer::empty() const
{
	return !size_;
}

inline void composite_buffer::push_back(char value)
{
	ensure_iovec_tail();

	static_cast<iovec_chunk *>(back_)->push_back(value);

	size_ += sizeof(char);
}

inline void composite_buffer::push_back(const char *value)
{
	push_back(value, std::strlen(value));
}

inline void composite_buffer::push_back(const std::string& value)
{
	ensure_iovec_tail();

	static_cast<iovec_chunk *>(back_)->push_back(value);

	size_ += value.size();
}

inline void composite_buffer::push_back(const void *buffer, int size)
{
	ensure_iovec_tail();

	static_cast<iovec_chunk *>(back_)->push_back(buffer, size);

	size_ += size;
}

template<typename PodType, std::size_t N>
inline void composite_buffer::push_back(PodType (&data)[N])
{
	push_back(static_cast<const void *>(data), N * sizeof(PodType));
}

inline void composite_buffer::push_back(int fd, off_t offset, size_t size, bool close)
{
	if (back_)
	{
		back_->next = new fd_chunk(fd, offset, size, close);
		back_ = back_->next;
	}
	else
	{
		front_ = back_ = new fd_chunk(fd, offset, size, close);
	}

	size_ += back_->size;
}

inline void composite_buffer::push_back(composite_buffer& buffer)
{
	if (back_)
	{
		back_->next = buffer.front_;
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

template<class Writer>
ssize_t composite_buffer::write(Writer writer)
{
	// TODO
	errno = ENOSYS;
	return -1;
}

template<class Writer, class CompletionHandler>
inline void composite_buffer::async_write(Writer writer, const CompletionHandler& handler)
{
	write_handler<Writer, CompletionHandler> internalWriteHandler(*this, writer, handler);

	writer.callback(internalWriteHandler);
}

inline void composite_buffer::ensure_iovec_tail()
{
	if (!back_)
	{
		front_ = back_ = new iovec_chunk();
	}
	else if (back_->type != chunk::ciov)
	{
		back_->next = new iovec_chunk();
		back_ = back_->next;
	}
}

template<class Writer, class CompletionHandler>
inline ssize_t composite_buffer::async_write_some(Writer writer, const CompletionHandler& handler)
{
	size_t nwritten = 0;
	ssize_t rv;

	if (front_)
	{
		rv = writer.write(*front_);

		if (rv != -1)
		{
			size_ -= rv;
			nwritten += rv;

#if 1
			if (!front_->size)
			{
				remove_front();
			}
#else
			while (!front_->size)
			{
				remove_front();

				rv = writer.write(*front_);

				if (rv != -1)
				{
					size_ -= rv;
					nwritten += rv;
				}
			}
#endif
		}
	}
	else
	{
		rv = 0;
	}

	writer.callback(handler);

	return rv != -1 ? nwritten : rv;
}
// }}}

} // namespace x0

#endif
