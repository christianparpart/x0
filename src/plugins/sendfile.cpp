/* <x0/mod_sendfile.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/FileSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/File.h>
#include <x0/strutils.h>
#include <x0/Types.h>

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
	public x0::HttpPlugin,
	public x0::IHttpRequestHandler
{
private:
public:
	sendfile_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		server_.onHandleRequest.connect(this);
	}

	~sendfile_plugin() {
		server_.onHandleRequest.disconnect(this);
	}

private:
	/**
	 * verifies wether the client may use its cache or not.
	 *
	 * \param in request object
	 * \param out response object. this will be modified in case of cache reusability.
	 */
	x0::http_error verify_client_cache(x0::HttpRequest *in, x0::HttpResponse *out) // {{{
	{
		std::string value;

		// If-None-Match, If-Modified-Since
		if ((value = in->header("If-None-Match")) != "")
		{
			if (value == in->fileinfo->etag())
			{
				if ((value = in->header("If-Modified-Since")) != "") // ETag + If-Modified-Since
				{
					x0::DateTime date(value);

					if (!date.valid())
						return x0::http_error::bad_request;

					if (in->fileinfo->mtime() <= date.unixtime())
						return x0::http_error::not_modified;
				}
				else // ETag-only
				{
					return x0::http_error::not_modified;
				}
			}
		}
		else if ((value = in->header("If-Modified-Since")) != "")
		{
			x0::DateTime date(value);
			if (!date.valid())
				return x0::http_error::bad_request;

			if (in->fileinfo->mtime() <= date.unixtime())
				return x0::http_error::not_modified;
		}

		return x0::http_error::ok;
	} // }}}

	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out) // {{{
	{
		if (!in->fileinfo->exists())
			return false;

		if (!in->fileinfo->is_regular())
			return false;

		out->status = verify_client_cache(in, out);
		if (out->status != x0::http_error::ok)
		{
			out->finish();
			return true;
		}

		x0::FilePtr f;
		if (equals(in->method, "GET"))
		{
			f.reset(new x0::File(in->fileinfo));

			if (f->handle() == -1)
			{
				server_.log(x0::Severity::error, "Could not open file '%s': %s",
					in->fileinfo->filename().c_str(), strerror(errno));

				out->status = x0::http_error::forbidden;
				out->finish();
				return true;
			}
		}
		else if (!equals(in->method, "HEAD"))
		{
			out->status = x0::http_error::method_not_allowed;
			out->finish();
			return true;
		}

		out->headers.push_back("Last-Modified", in->fileinfo->last_modified());
		out->headers.push_back("ETag", in->fileinfo->etag());

		if (!process_range_request(in, out, f))
		{
			out->headers.push_back("Accept-Ranges", "bytes");
			out->headers.push_back("Content-Type", in->fileinfo->mimetype());
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(in->fileinfo->size()));

			if (!f) // HEAD request
			{
				out->finish();
			}
			else
			{
				posix_fadvise(f->handle(), 0, in->fileinfo->size(), POSIX_FADV_SEQUENTIAL);

				out->write(
					std::make_shared<x0::FileSource>(f, 0, in->fileinfo->size()),
					std::bind(&x0::HttpResponse::finish, out)
				);
			}
		}
		return true;
	} // }}}

	inline bool process_range_request(x0::HttpRequest *in, x0::HttpResponse *out, x0::FilePtr& f) //{{{
	{
		x0::BufferRef range_value(in->header("Range"));
		x0::HttpRangeDef range;

		// if no range request or range request was invalid (by syntax) we fall back to a full response
		if (range_value.empty() || !range.parse(range_value))
			return false;

		out->status = x0::http_error::partial_content;

		if (range.size() > 1)
		{
			// generate a multipart/byteranged response, as we've more than one range to serve

			auto content = std::make_shared<x0::CompositeSource>();
			x0::Buffer buf;
			std::string boundary(boundary_generate());
			std::size_t content_length = 0;

			for (int i = 0, e = range.size(); i != e; ++i)
			{
				std::pair<std::size_t, std::size_t> offsets(make_offsets(range[i], in->fileinfo->size()));
				if (offsets.second < offsets.first)
				{
					out->status = x0::http_error::requested_range_not_satisfiable;
					return true;
				}

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

				if (f)
				{
					content->push_back(std::make_shared<x0::BufferSource>(std::move(buf)));
					content->push_back(std::make_shared<x0::FileSource>(f, offsets.first, length));
				}
				content_length += buf.size() + length;
			}

			buf.clear();
			buf.push_back("\r\n--");
			buf.push_back(boundary);
			buf.push_back("--\r\n");

			content->push_back(std::make_shared<x0::BufferSource>(std::move(buf)));
			content_length += buf.size();

			out->headers.push_back("Content-Type", "multipart/byteranges; boundary=" + boundary);
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(content_length));

			if (f)
			{
				out->write(content, std::bind(&x0::HttpResponse::finish, out));
			}
			else
			{
				out->finish();
			}
		}
		else
		{
			// generate a simple partial response

			std::pair<std::size_t, std::size_t> offsets(make_offsets(range[0], in->fileinfo->size()));
			if (offsets.second < offsets.first)
			{
				out->status = x0::http_error::requested_range_not_satisfiable;
				return true;
			}

			std::size_t length = 1 + offsets.second - offsets.first;

			out->headers.push_back("Content-Type", in->fileinfo->mimetype());
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(length));

			std::stringstream cr;
			cr << "bytes " << offsets.first << '-' << offsets.second << '/' << in->fileinfo->size();
			out->headers.push_back("Content-Range", cr.str());

			if (f)
			{
				out->write(
					std::make_shared<x0::FileSource>(f, offsets.first, length),
					std::bind(&x0::HttpResponse::finish, out)
				);
			}
			else
			{
				out->finish();
			}
		}

		return true;
	}//}}}

	std::pair<std::size_t, std::size_t> make_offsets(const std::pair<std::size_t, std::size_t>& p, std::size_t actual_size)
	{
		std::pair<std::size_t, std::size_t> q;

		if (p.first == x0::HttpRangeDef::npos) // suffix-range-spec
		{
			q.first = actual_size - p.second;
			q.second = actual_size - 1;
		}
		else
		{
			q.first = p.first;

			q.second = p.second == x0::HttpRangeDef::npos && p.second > actual_size
				? actual_size - 1
				: p.second;
		}

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
