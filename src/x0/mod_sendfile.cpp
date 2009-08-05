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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/* feature to detect origin mime types of backup files. */
#define X0_SENDFILE_MIME_TYPES_BELOW_BACKUP 1

/**
 * \ingroup modules
 * \brief serves static files from server's local filesystem to client.
 */
class sendfile_plugin :
	public x0::plugin
{
private:
	typedef std::map<std::string, std::string> mime_types_type;

	mime_types_type mime_types_;
	std::string default_mimetype_;
	bool etag_consider_mtime_;
	bool etag_consider_size_;
	bool etag_consider_inode_;
	std::map<const struct stat *, std::string> etag_cache_;

	x0::handler::connection c;

public:
	sendfile_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		mime_types_(),
		default_mimetype_("text/plain"),
		etag_consider_mtime_(true),
		etag_consider_size_(true),
		etag_consider_inode_(false),
		etag_cache_(),
		c()
	{
		c = server_.generate_content.connect(boost::bind(&sendfile_plugin::sendfile, this, _1, _2));
		server_.stat.on_invalidate.connect(boost::bind(&sendfile_plugin::etag_invalidate, this, _1, _2));
	}

	~sendfile_plugin() {
		server_.generate_content.disconnect(c);
	}

	virtual void configure()
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

		// mime-types loading
		std::string input(x0::read_file(server_.config().get("sendfile", "mime-types")));
		tokenizer lines(input, boost::char_separator<char>("\n"));

		for (tokenizer::iterator i = lines.begin(), e = lines.end(); i != e; ++i)
		{
			std::string line(x0::trim(*i));
			tokenizer columns(line, boost::char_separator<char>(" \t"));

			tokenizer::iterator ci = columns.begin(), ce = columns.end();
			std::string mime = ci != ce ? *ci++ : std::string();

			if (!mime.empty() && mime[0] != '#')
			{
				for (; ci != ce; ++ci)
				{
					mime_types_[*ci] = mime;
				}
			}
		}

		if ((input = server_.config().get("sendfile", "default-mime-type")) != "")
		{
			default_mimetype_ = input;
		}

		// ETag considerations
		if ((input = server_.config().get("sendfile", "etag-consider-mtime")) != "")
		{
			etag_consider_mtime_ = input == "true";
		}

		if ((input = server_.config().get("sendfile", "etag-consider-size")) != "")
		{
			etag_consider_size_ = input == "true";
		}

		if ((input = server_.config().get("sendfile", "etag-consider-inode")) != "")
		{
			etag_consider_inode_ = input == "true";
		}
	}

private:
	/**
	 * verifies wether the client may use its cache or not.
	 *
	 * \param in request object
	 * \param out response object. this will be modified in case of cache reusability.
	 * \param st stat structure of the requested entity.
	 *
	 * \throw response::not_modified, in case the client may use its cache.
	 */
	void verify_client_cache(x0::request& in, x0::response& out, struct stat *st)
	{
		// If-None-Match, If-Modified-Since

		std::string value;
		if ((value = in.header("If-None-Match")) != "")
		{
			if (value == etag_generate(st))
			{
				if ((value = in.header("If-Modified-Since")) != "") // ETag + If-Modified-Since
				{
					if (time_t date = from_http_date(value))
					{
						if (st->st_mtime <= date)
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
			if (time_t date = from_http_date(value))
			{
				if (st->st_mtime <= date)
				{
					throw x0::response::not_modified;
				}
			}
		}
	}

	inline time_t from_http_date(const std::string& value)
	{
		struct tm tm;
		tm.tm_isdst = 0;

		if (strptime(value.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm))
		{
			return mktime(&tm) - timezone;
		}

		return 0;
	}

	bool sendfile(x0::request& in, x0::response& out)
	{
		std::string path(in.entity);

		struct stat *st = in.connection.server().stat(path);
		if (st == 0)
			return false;

		if (!S_ISREG(st->st_mode))
			throw x0::response::forbidden;

		verify_client_cache(in, out, st);

		int fd = open(path.c_str(), O_RDONLY);
		if (fd == -1)
		{
			server_.log(x0::severity::error, "Could not open file '%s': %s",
				path.c_str(), strerror(errno));

			return false;
		}

		try
		{
			out.header("Last-Modified", x0::http_date(st->st_mtime));
			out.header("ETag", etag_generate(st));

			if (!process_range_request(in, out, st, fd))
			{
				out.status = x0::response::ok;

				out.header("Accept-Ranges", "bytes");
				out.header("Content-Type", get_mime_type(in));
				out.header("Content-Length", boost::lexical_cast<std::string>(st->st_size));

				posix_fadvise(fd, 0, st->st_size, POSIX_FADV_SEQUENTIAL);
				out.write(fd, 0, st->st_size, true);
			}

			out.flush();
		}
		catch (...)
		{
			::close(fd);	// we would let auto-close it by last fd write,
							// however, this won't happen when an exception got cought here.

			throw;
		}

		return true;
	}

	inline bool process_range_request(x0::request& in, x0::response& out, struct stat *st, int fd)
	{
		std::string range_value(in.header("Range"));
		x0::range_def range;

		// if no range request or range request was invalid (by syntax) we fall back to a full response
		if (range_value.empty() || !range.parse(range_value))
			return false;

		std::string mimetype(get_mime_type(in));

		out.status = x0::response::partial_content;

		if (range.size() > 1)
		{
			// generate a multipart/byteranged response, as we've more than one range to serve

			x0::composite_buffer body;
			std::string boundary(boundary_generate());

			for (int i = 0, e = range.size(); i != e; )
			{
				std::pair<std::size_t, std::size_t> offsets(make_offsets(range[i], st));
				std::size_t length = 1 + offsets.second - offsets.first;

				body.push_back("\r\n--");
				body.push_back(boundary);

				body.push_back("\r\nContent-Type: ");
				body.push_back(mimetype);

				body.push_back("\r\nContent-Range: bytes ");
				body.push_back(boost::lexical_cast<std::string>(offsets.first));
				body.push_back("-");
				body.push_back(boost::lexical_cast<std::string>(offsets.second));
				body.push_back("/");
				body.push_back(boost::lexical_cast<std::string>(st->st_size));
				body.push_back("\r\n\r\n");

				body.push_back(fd, offsets.first, length, ++i == e);
			}
			body.push_back("\r\n--");
			body.push_back(boundary);
			body.push_back("--\r\n");

			out.header("Content-Type", "multipart/byteranges; boundary=" + boundary);
			out.header("Content-Length", boost::lexical_cast<std::string>(body.size()));

			out.write(body);
		}
		else
		{
			// generate a simple partial response

			std::pair<std::size_t, std::size_t> offsets(make_offsets(range[0], st));
			std::size_t length = 1 + offsets.second - offsets.first;

			out.header("Content-Type", get_mime_type(in));
			out.header("Content-Length", boost::lexical_cast<std::string>(length));

			std::stringstream cr;
			cr << "bytes " << offsets.first << '-' << offsets.second << '/' << st->st_size;
			out.header("Content-Range", cr.str());

			out.write(fd, offsets.first, length, true);
		}

		return true;
	}

	std::pair<std::size_t, std::size_t> make_offsets(const std::pair<std::size_t, std::size_t>& p, const struct stat *st)
	{
		std::pair<std::size_t, std::size_t> q;

		if (p.first == x0::range_def::npos) // suffix-range-spec
		{
			q.first = st->st_size - p.second;
			q.second = st->st_size - 1;
		}
		else
		{
			q.first = p.first;

			q.second = p.second == x0::range_def::npos && p.second > std::size_t(st->st_size)
				? st->st_size - 1
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

	/**
	 * generates an ETag for given inode.
	 * \param st stat structure to generate the ETag for.
	 * \return an HTTP/1.1 conform ETag value.
	 */
	inline std::string etag_generate(const struct stat *st)
	{
		{
			auto cache_entry = etag_cache_.find(st);

			if (cache_entry != etag_cache_.end())
				return cache_entry->second;
		}

		std::stringstream sstr;
		int count = 0;

		sstr << '"';

		if (etag_consider_mtime_)
		{
			++count;
			sstr << st->st_mtime;
		}

		if (etag_consider_size_)
		{
			if (count++) sstr << '-';
			sstr << st->st_size;
		}

		if (etag_consider_inode_)
		{
			if (count++) sstr << '-';
			sstr << st->st_ino;
		}

		sstr << '"';

		return etag_cache_[st] = sstr.str();
	}

	void etag_invalidate(const std::string& filename, const struct stat *st)
	{
		etag_cache_.erase(etag_cache_.find(st));
	}

	/** computes the mime-type(/content-type) for given request.
	 * \param in the request to detect the mime-type for.
	 * \return mime-type for given request.
	 */
	inline std::string get_mime_type(x0::request& in) const
	{
		std::size_t ndot = in.entity.find_last_of(".");
		std::size_t nslash = in.entity.find_last_of("/");

		if (ndot != std::string::npos && ndot > nslash)
		{
			return get_mime_type(in.entity.substr(ndot + 1));
		}
		else
		{
			return default_mimetype_;
		}
	}

	inline std::string get_mime_type(std::string ext) const
	{
		while (ext.size())
		{
			mime_types_type::const_iterator i = mime_types_.find(ext);

			if (i != mime_types_.end())
			{
				return i->second;
			}
#if X0_SENDFILE_MIME_TYPES_BELOW_BACKUP
			if (ext[ext.size() - 1] != '~')
			{
				break;
			}

			ext.resize(ext.size() - 1);
#else
			break;
#endif
		}

		return default_mimetype_;
	}
};

X0_EXPORT_PLUGIN(sendfile);
