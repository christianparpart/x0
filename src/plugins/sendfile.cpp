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

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>

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
	x0::handler::connection c;

public:
	sendfile_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		c()
	{
		c = server_.generate_content.connect(boost::bind(&sendfile_plugin::sendfile, this, _1, _2));
	}

	~sendfile_plugin() {
		server_.generate_content.disconnect(c);
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
	void verify_client_cache(x0::request& in, x0::response& out)
	{
		// If-None-Match, If-Modified-Since

		std::string value;
		if ((value = in.header("If-None-Match")) != "")
		{
			if (value == in.fileinfo->etag())
			{
				if ((value = in.header("If-Modified-Since")) != "") // ETag + If-Modified-Since
				{
					x0::datetime date(value);

					if (date.valid())
					{
						if (in.fileinfo->mtime() <= date.unixtime())
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
		else if ((value = in.header("If-Modified-Since")) != "")
		{
			x0::datetime date(value);

			if (date.valid())
			{
				if (in.fileinfo->mtime() <= date.unixtime())
				{
					throw x0::response::not_modified;
				}
			}
		}
	}

	enum method_type {
		HEAD,
		GET
	};

	bool sendfile(x0::request& in, x0::response& out)
	{
		std::string path(in.fileinfo->filename());

		if (!in.fileinfo->exists())
			return false;

		if (!in.fileinfo->is_regular())
			return false;

		verify_client_cache(in, out);

		out.header("Last-Modified", in.fileinfo->last_modified());
		out.header("ETag", in.fileinfo->etag());

		int fd;
		if (equals(in.method, "GET"))
		{
			if ((fd = open(path.c_str(), O_RDONLY)) == -1)
			{
				server_.log(x0::severity::error, "Could not open file '%s': %s", path.c_str(), strerror(errno));
				return false;
			}
		}
		else if (equals(in.method, "HEAD"))
			fd = -1;
		else
			return false;

		try
		{
			if (!process_range_request(in, out, fd))
			{
				out.status = x0::response::ok;

				out.header("Accept-Ranges", "bytes");
				out.header("Content-Type", in.fileinfo->mimetype());
				out.header("Content-Length", boost::lexical_cast<std::string>(in.fileinfo->size()));

				if (fd != -1)
				{
					posix_fadvise(fd, 0, in.fileinfo->size(), POSIX_FADV_SEQUENTIAL);
					out.write(fd, 0, in.fileinfo->size(), true);
				}
			}

			out.flush();
		}
		catch (...)
		{
			if (fd != -1)
				::close(fd);// we would let auto-close it by last fd write,
							// however, this won't happen when an exception got cought here.

			throw;
		}

		return true;
	}

	inline bool process_range_request(x0::request& in, x0::response& out, int fd)
	{
		x0::buffer_ref range_value(in.header("Range"));
		x0::range_def range;

		// if no range request or range request was invalid (by syntax) we fall back to a full response
		if (range_value.empty() || !range.parse(range_value))
			return false;

		out.status = x0::response::partial_content;

		if (range.size() > 1)
		{
			// generate a multipart/byteranged response, as we've more than one range to serve

			x0::composite_buffer body;
			std::string boundary(boundary_generate());

			for (int i = 0, e = range.size(); i != e; )
			{
				std::pair<std::size_t, std::size_t> offsets(make_offsets(range[i], in.fileinfo->size()));
				std::size_t length = 1 + offsets.second - offsets.first;

				body.push_back("\r\n--");
				body.push_back(boundary);

				body.push_back("\r\nContent-Type: ");
				body.push_back(in.fileinfo->mimetype());

				body.push_back("\r\nContent-Range: bytes ");
				body.push_back(boost::lexical_cast<std::string>(offsets.first));
				body.push_back("-");
				body.push_back(boost::lexical_cast<std::string>(offsets.second));
				body.push_back("/");
				body.push_back(boost::lexical_cast<std::string>(in.fileinfo->size()));
				body.push_back("\r\n\r\n");

				body.push_back(fd, offsets.first, length, ++i == e);
			}
			body.push_back("\r\n--");
			body.push_back(boundary);
			body.push_back("--\r\n");

			out.header("Content-Type", "multipart/byteranges; boundary=" + boundary);
			out.header("Content-Length", boost::lexical_cast<std::string>(body.size()));

			if (fd != -1)
			{
				out.write(body);
			}
		}
		else
		{
			// generate a simple partial response

			std::pair<std::size_t, std::size_t> offsets(make_offsets(range[0], in.fileinfo->size()));
			std::size_t length = 1 + offsets.second - offsets.first;

			out.header("Content-Type", in.fileinfo->mimetype());
			out.header("Content-Length", boost::lexical_cast<std::string>(length));

			std::stringstream cr;
			cr << "bytes " << offsets.first << '-' << offsets.second << '/' << in.fileinfo->size();
			out.header("Content-Range", cr.str());

			if (fd != -1)
			{
				out.write(fd, offsets.first, length, true);
			}
		}

		return true;
	}

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
