#include <x0/io/AsyncWriter.h>
#include <x0/io/ConnectionSink.h>
#include <x0/http/HttpConnection.h>
#include <x0/Types.h>
#include <ev++.h>
#include <memory>

#if 1
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("AsyncWriter: " msg)
#endif

namespace x0 {

class AsyncWriter // {{{
{
private:
	std::shared_ptr<ConnectionSink> sink_;
	SourcePtr source_;
	CompletionHandlerType handler_;
	std::size_t bytes_transferred_;

public:
	AsyncWriter(std::shared_ptr<ConnectionSink> snk, const SourcePtr& src, const CompletionHandlerType& handler) :
		sink_(snk),
		source_(src),
		handler_(handler),
		bytes_transferred_(0)
	{
	}

	~AsyncWriter()
	{
	}

public:
	static void write(const std::shared_ptr<ConnectionSink>& snk, const SourcePtr& src, const CompletionHandlerType& handler)
	{
		AsyncWriter *writer = new AsyncWriter(snk, src, handler);
		writer->write();
	}

private:
	void finish(int rv)
	{
		// unregister from connection's on_write_ready handler
		sink_->connection()->stop_write();

		// invoke completion handler (this may have deleted our sink above)
		handler_(rv, bytes_transferred_);

		delete this;
	}

	void callback(HttpConnection *)
	{
		write();
	}

	void write()
	{
#if !defined(NTRACE)
		for (int i = 0; ; ++i)
#else
		for (;;)
#endif
		{
			ssize_t rv = sink_->pump(*source_); // true=complete,false=error,det=partial
			//TRACE("writer(%p).pump: %ld; %s", this, rv, rv < 0 ? strerror(errno) : "");

			if (rv > 0)
			{
				TRACE("(%p): write chunk done (%i)", this, i);
				// we wrote something (if not even all)
				bytes_transferred_ += rv;
			}
			else if (rv == 0)
			{
				TRACE("(%p): write complete (%i)", this, i);
				// finished in success
				finish(0);
				break;
			}
			else if (errno == EAGAIN || errno == EINTR)
			{
				TRACE("(%p): write incomplete (EINT|EAGAIN) (%i)", this, i);
				// call back as soon as sink is ready for more writes
				sink_->connection()->on_write_ready(std::bind(&AsyncWriter::callback, this, std::placeholders::_1));
				break;
			}
			else
			{
				TRACE("(%p): write failed: %s (%i)", this, strerror(errno), i);
				// an error occurred
				finish(errno);
				break;
			}
		}
	}
}; // }}}

void writeAsync(HttpConnection *target, const SourcePtr& source, const CompletionHandlerType& completionHandler)
{
	writeAsync(std::make_shared<ConnectionSink>(target), source, completionHandler);
}

void writeAsync(const std::shared_ptr<ConnectionSink>& target, const SourcePtr& source, const CompletionHandlerType& completionHandler)
{
	AsyncWriter::write(target, source, completionHandler);
}

} // namespace x0
