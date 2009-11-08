/* <x0/composite_buffer_async_writer.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */
#ifndef composite_buffer_async_writer_hpp
#define composite_buffer_async_writer_hpp 1

#include <x0/sysconfig.h>
#include <x0/api.hpp>
#include <x0/detail/scoped_mmap.hpp>
#include <asio.hpp>
#include <boost/shared_ptr.hpp>

namespace x0 {

/**
 *
 * function object class for writing a composite_buffer to a target.
 *
 * The template `Target` must support:
 * <ul>
 *   <li>`ssize_t write(void *buffer, std::size_t size)` - writes buffer of given size to underlying device</li>
 *   <li>`void async_write_some(asio::null_buffers(), completionHandler)` - calles back on write-readyness with nul_buffers() passed as first argument</li>
 *   <li>`int native() const` - to return the underlying device handle (file descriptor)</li>
 * </ul>
 */
template<class Target, class CompletionHandler>
class composite_buffer_async_writer :
	public composite_buffer::visitor
{
private:
	struct context
	{
		Target& target_;
		composite_buffer cb_;
		CompletionHandler handler_;

		const composite_buffer::chunk *current_;
		std::size_t offset_;
		ssize_t status_;
		std::size_t nwritten_;
		std::size_t row_slice_, col_slice_;

		context(Target& target, const composite_buffer& cb, CompletionHandler handler) :
			target_(target), cb_(cb), handler_(handler),
			current_(cb_.front()), offset_(0), status_(0), nwritten_(0), row_slice_(0), col_slice_(0)
		{
		}
	};
	boost::shared_ptr<context> context_;

public:
	composite_buffer_async_writer(Target& t, const composite_buffer& cb, CompletionHandler handler);

	void operator()();
	void operator()(const asio::error_code& ec, std::size_t bytes_transferred);

private:
	bool write_some_once();
	void async_write_some();

	virtual void visit(const composite_buffer::iovec_chunk&);
	virtual void visit(const composite_buffer::fd_chunk&);
};

/** 
 * Initiates an asynchronous write operation.
 *
 * \param target the target object that receives the data to be written.
 * \param source the source object whose data has to be written to \p target.
 * \param handler the completion handler to be invoked once the write operation has been completed by success or aborted by an error.
 *
 * Initiates an asynchronous write on \p target to write the \p source and will invoke
 * the completion \p handler once done (or an error occured).
 */
template<class Target, class CompletionHandler>
void async_write(Target& target, const composite_buffer& source, CompletionHandler handler);

// {{{ impl
template<class Target, class CompletionHandler>
inline composite_buffer_async_writer<Target, CompletionHandler>::composite_buffer_async_writer(Target& target, const composite_buffer& cb, CompletionHandler handler) :
	context_(new context(target, cb, handler))
{
}

/**
 * initiates the asynchronous write operation.
 */
template<class Target, class CompletionHandler>
inline void composite_buffer_async_writer<Target, CompletionHandler>::operator()()
{
	context_->target_.async_write_some(asio::null_buffers(), *this);
}

/**
 * this method gets invoked once the target becomes ready for non-blocking writes, and though will write "some" data.
 *
 * \param ec the system error code for the previous system write operation.
 * \param bytes_transferred the number of bytes the previous system write operation actually wrote.
 *
 * If all data has been written, the completion handler gets invoked, otherwise,
 * this object will invoke itself again once the target becomes write-ready again.
 */
template<class Target, class CompletionHandler>
inline void composite_buffer_async_writer<Target, CompletionHandler>::operator()(const asio::error_code& ec, std::size_t bytes_transferred)
{
	if (!ec && context_->nwritten_ < context_->cb_.size())
	{
		async_write_some();
	}
	else
	{
		context_->handler_(ec, context_->nwritten_);
	}
}

/**
 * writes some data into the target object and invokes itself as function object when target is ready for a next write.
 *
 * \return the status code of the last write action.
 */
template<class Target, class CompletionHandler>
inline void composite_buffer_async_writer<Target, CompletionHandler>::async_write_some()
{
	if (!write_some_once())
	{
		// something ill happened when writing
		return;
	}

	// loop through all chunk-writes long as they're complete writes
	while (context_->offset_ == context_->current_->size())
	{
		context_->offset_ = 0;
		context_->current_ = context_->current_->next();

		if (!context_->current_)
		{
			// composite_buffer fully written.
			context_->handler_(asio::error_code(), context_->nwritten_);
			return;
		}

		if (!write_some_once())
		{
			// something ill happened when writing
			return;
		}
	}

	// callback when target is ready for more writes
	context_->target_.async_write_some(asio::null_buffers(), *this);
}

/**
 * Writes some buffer from the current chunk into the target.
 *
 * \retval true the write succeed (wether partial or complete).
 * \retval false the write failed and system's errno SHOULD be set appropriately; the completion handler has been informed about this error.
 *
 * If the write succeed, internal \p offset_ and \p nwritten_ properties are updated appropriately.
 * The \p status_ property will reflect the return code of the inners OS-level write call.
 */
template<class Target, class CompletionHandler>
inline bool composite_buffer_async_writer<Target, CompletionHandler>::write_some_once()
{
	context_->current_->accept(*this);

	if (context_->status_ != -1)
	{
		context_->offset_ += context_->status_;
		context_->nwritten_ += context_->status_;

		return true;
	}

	// XXX inform the completion-handler about the write error.
	context_->handler_(asio::error_code(errno, asio::error::system_category), context_->nwritten_);

	return false;
}

/** Writes contents from an iovec chunk into the target.
 */
template<class Target, class CompletionHandler>
inline void composite_buffer_async_writer<Target, CompletionHandler>::visit(const composite_buffer::iovec_chunk& chunk)
{
	#define DPRINTF if (context_->target_.native() == 10) printf
	#define INC(p, n) (p) = ((char *)(p)) + (n);
	#define DEC(p, n) (p) = ((char *)(p)) - (n);

	const_cast<iovec&>(chunk[context_->row_slice_]).iov_len -= context_->col_slice_;
	INC(const_cast<iovec&>(chunk[context_->row_slice_]).iov_base, context_->col_slice_);

	DPRINTF("writev: fd=%d, row=%ld, num_rows=%ld, diff=%ld, col=%ld, offset=%ld, i0:%ld\n",
		context_->target_.native(), 
		context_->row_slice_, chunk.length(),
		chunk.length() - context_->row_slice_,
		context_->col_slice_,
		context_->offset_,
		chunk[context_->row_slice_].iov_len);
	ssize_t nwritten = ::writev(context_->target_.native(),
		&chunk[context_->row_slice_], chunk.length() - context_->row_slice_);

	DEC(const_cast<iovec&>(chunk[context_->row_slice_]).iov_base, context_->col_slice_);
	const_cast<iovec&>(chunk[context_->row_slice_]).iov_len += context_->col_slice_;

	if (nwritten < 0)
		perror("writev");
	else
		DPRINTF("    nwritten=%ld, nmax=%ld/%ld, nvecs=%ld\n",
			nwritten,
			context_->cb_.size() - context_->nwritten_,
			context_->cb_.size(),
			chunk.length());

	context_->status_ = nwritten;

	if (nwritten > 0 &&
		static_cast<std::size_t>(nwritten) < chunk.size() - context_->nwritten_ - context_->col_slice_)
	{
		if (context_->col_slice_ + nwritten < chunk[context_->row_slice_].iov_len)
		{
			// number of actually written bytes would not exceed the current row.
			context_->col_slice_ += nwritten;
			nwritten = 0;
		}
		else
		{
			// search for new row/col slice
			DPRINTF("    sub %ld bytes from row %ld\n", chunk[context_->row_slice_].iov_len - context_->col_slice_, context_->row_slice_);
			nwritten -= chunk[context_->row_slice_++].iov_len - context_->col_slice_;

			do {
				DPRINTF("    sub %ld bytes from row %ld\n", chunk[context_->row_slice_].iov_len, context_->row_slice_);
				nwritten -= chunk[context_->row_slice_++].iov_len;
			}
			while (nwritten > 0);

			context_->col_slice_ = -nwritten;
		}
		DPRINTF("    new offsets: %ld, %ld; nremaining=%ld\n",
			context_->row_slice_, context_->col_slice_,
			nwritten);
	}
}

/** Writes contents from a file descriptor chunk into the target.
 */
template<class Target, class CompletionHandler>
inline void composite_buffer_async_writer<Target, CompletionHandler>::visit(const composite_buffer::fd_chunk& chunk)
{
	off_t offset = chunk.offset() + context_->offset_;
	std::size_t size = chunk.size() - context_->offset_;
	int fd = chunk.fd();

#if defined(HAVE_SENDFILE)
	int rv = ::sendfile(context_->target_.native(), fd, &offset, size);
#else
	char buf[8 * 1024];
	int rv = ::pread(fd, buf_, sizeof(buf_), offset);
	if (rv > 0)
	{
		rv = ::write(context_->target_.native(), buf, rv);
	}
#endif

	if (rv > 0 && size > std::size_t(rv))
		posix_fadvise(fd, offset, 0, POSIX_FADV_WILLNEED);

	context_->status_ = rv;
}

template<class Target, class CompletionHandler>
inline void async_write(Target& target, const composite_buffer& source, CompletionHandler handler)
{
	if (source.size() > 1)
	{
		int flag = 1;
		setsockopt(target.native(), IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
	}

	composite_buffer_async_writer<Target, CompletionHandler>(target, source, handler)();
}
// }}}

} // namespace x0

#endif
