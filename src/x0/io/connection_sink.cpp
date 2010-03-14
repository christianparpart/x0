#include <x0/connection.hpp>
#include <x0/io/connection_sink.hpp>
#include <x0/io/source.hpp>
#include <x0/io/file_source.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/io/filter_source.hpp>
#include <x0/io/composite_source.hpp>
#include <x0/sysconfig.h>

#if defined(HAVE_SYS_SENDFILE_H)
#	include <sys/sendfile.h>
#endif

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

namespace x0 {

connection_sink::connection_sink(x0::connection *conn) :
	fd_sink(conn->handle()),
	connection_(conn),
	rv_(0)
{
}

ssize_t connection_sink::pump(source& src)
{
#if defined(WITH_SSL)
	if (connection_->ssl_enabled())
	{
		if (buf_.empty())
			src.pull(buf_);

		std::size_t remaining = buf_.size() - offset_;
		if (!remaining)
			return 0;

		ssize_t nwritten = ::gnutls_write(connection_->ssl_session_, buf_.data() + offset_, remaining);

		if (nwritten != -1)
		{
			if (static_cast<std::size_t>(nwritten) == remaining)
			{
				buf_.clear();
				offset_ = 0;
			}
			else
				offset_ += nwritten;
		}

		return static_cast<std::size_t>(nwritten);
	}
	else
		src.accept(*this);
#else
	// call pump-handler
	src.accept(*this);
#endif

	return rv_;
}

void connection_sink::visit(fd_source& v)
{
	rv_ = fd_sink::pump(v);
}

void connection_sink::visit(file_source& v)
{
#if defined(HAVE_SENDFILE)
	if (!offset_)
		offset_ = v.offset(); // initialize with the starting-offset of interest

	if (std::size_t remaining = v.count() - offset_) // how many bytes are still to process
		rv_ = sendfile(handle(), v.handle(), &offset_, remaining);
	else
		rv_ = 0;
#else
	rv_ = fd_sink::pump(v);
#endif
}

void connection_sink::visit(buffer_source& v)
{
	rv_ = fd_sink::pump(v);
}

void connection_sink::visit(filter_source& v)
{
	rv_ = fd_sink::pump(v);
}

void connection_sink::visit(composite_source& v)
{
	rv_ = fd_sink::pump(v);
}

} // namespace x0
