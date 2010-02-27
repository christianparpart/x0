/* <x0/mod_sendfile.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/range_def.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <x0/io/file_source.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/io/composite_source.hpp>
#include <x0/io/file.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>

#include <sstream>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/* feature to detect origin mime types of backup files. */
#define X0_SENDFILE_MIME_TYPES_BELOW_BACKUP 1

/**
 * \ingroup plugins
 * \brief serves static files from server's local filesystem to client.
 */
class sendfile_plugin :
	public x0::plugin
{
private:
	x0::request_handler::connection c;

public:
	sendfile_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		c()
	{
		c = server_.generate_content.connect(&sendfile_plugin::sendfile, this);
	}

	~sendfile_plugin() {
		c.disconnect();
	}

	virtual void configure()
	{
	}

private:
	/**
	 * verifies wether the client may use its cache or not.
	 *
	 * \param in request object
	 * \param out response object. this will be modified in case of cache reusability.
	 *
	 * \throw response::not_modified, in case the client may use its cache.
	 */
	void verify_client_cache(x0::request *in, x0::response *out) // {{{
	{
		// If-None-Match, If-Modified-Since

		std::string value;
		if ((value = in->header("If-None-Match")) != "")
		{
			if (value == in->fileinfo->etag())
			{
				if ((value = in->header("If-Modified-Since")) != "") // ETag + If-Modified-Since
				{
					x0::datetime date(value);

					if (date.valid())
					{
						if (in->fileinfo->mtime() <= date.unixtime())
						{
							throw x0::response::not_modified;
						}
					}
				}
				else // ETag-only
				{
					throw x0::response::not_modified;
				}
			}
		}
		else if ((value = in->header("If-Modified-Since")) != "")
		{
			x0::datetime date(value);

			if (date.valid())
			{
				if (in->fileinfo->mtime() <= date.unixtime())
				{
					throw x0::response::not_modified;
				}
			}
		}
	} // }}}

	enum method_type {
		HEAD,
		GET
	};

	void sendfile(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out) // {{{
	{
		std::string path(in->fileinfo->filename());

		if (!in->fileinfo->exists())
			return next();

		if (!in->fileinfo->is_regular())
			return next();

		verify_client_cache(in, out);

		out->headers.push_back("Last-Modified", in->fileinfo->last_modified());
		out->headers.push_back("ETag", in->fileinfo->etag());

		x0::file_ptr f;
		if (equals(in->method, "GET"))
		{
			f.reset(new x0::file(in->fileinfo));

			if (f->handle() == -1)
			{
				server_.log(x0::severity::error, "Could not open file '%s': %s", path.c_str(), strerror(errno));
				return next();
			}
		}
		else if (!equals(in->method, "HEAD"))
			return next();

		if (!process_range_request(next, in, out, f))
		{
			out->status = x0::response::ok;

			out->headers.push_back("Accept-Ranges", "bytes");
			out->headers.push_back("Content-Type", in->fileinfo->mimetype());
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(in->fileinfo->size()));

			if (!f)
				return next.done();

			posix_fadvise(f->handle(), 0, in->fileinfo->size(), POSIX_FADV_SEQUENTIAL);

			out->write(
				std::make_shared<x0::file_source>(f, 0, in->fileinfo->size()),
				std::bind(&sendfile_plugin::done, this, next)
			);
		}
	} // }}}

	void done(x0::request_handler::invokation_iterator next)
	{
		next.done();
	}

	inline bool process_range_request(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out, x0::file_ptr& f) //{{{
	{
		x0::buffer_ref range_value(in->header("Range"));
		x0::range_def range;

		// if no range request or range request was invalid (by syntax) we fall back to a full response
		if (range_value.empty() || !range.parse(range_value))
			return false;

		out->status = x0::response::partial_content;

		if (range.size() > 1)
		{
			// generate a multipart/byteranged response, as we've more than one range to serve

			auto content = std::make_shared<x0::composite_source>();
			x0::buffer buf;
			std::string boundary(boundary_generate());
			std::size_t content_length = 0;

			for (int i = 0, e = range.size(); i != e; ++i)
			{
				std::pair<std::size_t, std::size_t> offsets(make_offsets(range[i], in->fileinfo->size()));
				std::size_t length = 1 + offsets.second - offsets.first;

				buf.clear();
				buf.push_back("\r\n--");
				buf.push_back(boundary);
				buf.push_back("\r\nContent-Type: ");
				buf.push_back(in->fileinfo->mimetype());

				buf.push_back("\r\nContent-Range: bytes ");
				buf.push_back(boost::lexical_cast<std::string>(offsets.first));
				buf.push_back("-");
				buf.push_back(boost::lexical_cast<std::string>(offsets.second));
				buf.push_back("/");
				buf.push_back(boost::lexical_cast<std::string>(in->fileinfo->size()));
				buf.push_back("\r\n\r\n");

				content->push_back(std::make_shared<x0::buffer_source>(buf));
				content->push_back(std::make_shared<x0::file_source>(f, offsets.first, length));
				content_length += buf.size() + length;
			}

			buf.clear();
			buf.push_back("\r\n--");
			buf.push_back(boundary);
			buf.push_back("--\r\n");

			content->push_back(std::make_shared<x0::buffer_source>(buf));
			content_length += buf.size();

			out->headers.push_back("Content-Type", "multipart/byteranges; boundary=" + boundary);
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(content_length));

			if (f)
			{
				out->write(content, std::bind(&sendfile_plugin::done, this, next));
			}
		}
		else
		{
			// generate a simple partial response

			std::pair<std::size_t, std::size_t> offsets(make_offsets(range[0], in->fileinfo->size()));
			std::size_t length = 1 + offsets.second - offsets.first;

			out->headers.push_back("Content-Type", in->fileinfo->mimetype());
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(length));

			std::stringstream cr;
			cr << "bytes " << offsets.first << '-' << offsets.second << '/' << in->fileinfo->size();
			out->headers.push_back("Content-Range", cr.str());

			if (f)
			{
				out->write(
					std::make_shared<x0::file_source>(f, offsets.first, length),
					std::bind(&sendfile_plugin::done, this, next)
				);
			}
		}

		return true;
	}//}}}

	std::pair<std::size_t, std::size_t> make_offsets(const std::pair<std::size_t, std::size_t>& p, std::size_t actual_size)
	{
		std::pair<std::size_t, std::size_t> q;

		if (p.first == x0::range_def::npos) // suffix-range-spec
		{
			q.first = actual_size - p.second;
			q.second = actual_size - 1;
		}
		else
		{
			q.first = p.first;

			q.second = p.second == x0::range_def::npos && p.second > actual_size
				? actual_size - 1
				: p.second;
		}

		if (q.second < q.first)
			throw x0::response::requested_range_not_satisfiable;

		return q;
	}

	/**
	 * generates a boundary tag.
	 *
	 * \return a value usable as boundary tag.
	 */
	inline std::string boundary_generate() const
	{
		static const char *map = "0123456789abcdef";
		char buf[16 + 1];

		for (std::size_t i = 0; i < sizeof(buf) - 1; ++i)
			buf[i] = map[random() % (sizeof(buf) - 1)];

		buf[sizeof(buf) - 1] = '\0';

		return std::string(buf);
	}
};

X0_EXPORT_PLUGIN(sendfile);
