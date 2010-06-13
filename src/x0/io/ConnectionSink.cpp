#include <x0/http/HttpConnection.h>
#include <x0/io/ConnectionSink.h>
#include <x0/io/Source.h>
#include <x0/io/FileSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FilterSource.h>
#include <x0/io/CompositeSource.h>
#include <x0/sysconfig.h>

#if defined(HAVE_SYS_SENDFILE_H)
#	include <sys/sendfile.h>
#endif

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

namespace x0 {

ConnectionSink::ConnectionSink(HttpConnection *conn) :
	SystemSink(conn->handle()),
	connection_(conn),
	rv_(0)
{
}

ssize_t ConnectionSink::pump(Source& src)
{
#if defined(WITH_SSL)
	if (connection_->isSecure())
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
		return nwritten;
	}
	else
		src.accept(*this);
#else
	// call pump-handler
	src.accept(*this);
#endif

	return rv_;
}

void ConnectionSink::visit(SystemSource& v)
{
	rv_ = SystemSink::pump(v);
}

void ConnectionSink::visit(FileSource& v)
{
#if defined(HAVE_SENDFILE)
	if (!offset_)
		offset_ = v.offset(); // initialize with the starting-offset of interest

	if (std::size_t remaining = v.count() - offset_) // how many bytes are still to process
		rv_ = sendfile(handle(), v.handle(), &offset_, remaining);
	else
		rv_ = 0;
#else
	rv_ = SystemSink::pump(v);
#endif
}

void ConnectionSink::visit(BufferSource& v)
{
	rv_ = SystemSink::pump(v);
}

void ConnectionSink::visit(FilterSource& v)
{
	rv_ = SystemSink::pump(v);
}

void ConnectionSink::visit(CompositeSource& v)
{
	rv_ = SystemSink::pump(v);
}

} // namespace x0
