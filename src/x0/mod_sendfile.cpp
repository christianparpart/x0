/* <x0/mod_sendfile.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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

	x0::handler::connection c;

public:
	sendfile_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		mime_types_(),
		default_mimetype_("text/plain"),
		etag_consider_mtime_(true),
		etag_consider_size_(true),
		etag_consider_inode_(false),
		c()
	{
		c = server_.generate_content.connect(boost::bind(&sendfile_plugin::sendfile, this, _1, _2));
	}

	~sendfile_plugin() {
		server_.generate_content.disconnect(c);
	}

	virtual void configure()
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

		// mime-types loading
		std::string input(x0::read_file(server_.get_config().get("sendfile", "mime-types")));
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

		if ((input = server_.get_config().get("sendfile", "default-mime-type")) != "")
		{
			default_mimetype_ = input;
		}

		// ETag considerations
		if ((input = server_.get_config().get("sendfile", "etag-consider-mtime")) != "")
		{
			etag_consider_mtime_ = input == "true";
		}

		if ((input = server_.get_config().get("sendfile", "etag-consider-size")) != "")
		{
			etag_consider_size_ = input == "true";
		}

		if ((input = server_.get_config().get("sendfile", "etag-consider-inode")) != "")
		{
			etag_consider_inode_ = input == "true";
		}
	}

private:
	bool sendfile(x0::request& in, x0::response& out) {
		std::string path(in.entity);

		struct stat st;
		if (stat(path.c_str(), &st) != 0)
			return false;

		int fd = open(path.c_str(), O_RDONLY);
		if (fd == -1)
		{
			server_.log(__FILENAME__, __LINE__, x0::severity::error, "Could not open file '%s': %s",
				path.c_str(), strerror(errno));

			return false;
		}

		out.header("Content-Type", get_mime_type(in));
		out.header("Content-Length", boost::lexical_cast<std::string>(st.st_size));
		out.header("Last-Modified", x0::http_date(st.st_mtime));
		out.header("ETag", etag_generate(st));
		// TODO: set other related response headers...

		out.write(fd, 0, st.st_size, true);

		// XXX send out headers, as they're fixed size in user space.
		// XXX start async transfer through sendfile()

		out.flush();

		return true;
	}

	/**
	 * generates an ETag for given inode.
	 * \param st stat structure to generate the ETag for.
	 * \return an HTTP/1.1 conform ETag value.
	 */
	inline std::string etag_generate(const struct stat& st)
	{
		std::stringstream sstr;
		int count = 0;

		sstr << '"';

		if (etag_consider_mtime_)
		{
			++count;
			sstr << st.st_mtime;
		}

		if (etag_consider_size_)
		{
			if (count++) sstr << '-';
			sstr << st.st_size;
		}

		if (etag_consider_inode_)
		{
			if (count++) sstr << '-';
			sstr << st.st_ino;
		}

		sstr << '"';

		return sstr.str();
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

extern "C" x0::plugin *sendfile_init(x0::server& srv, const std::string& name) {
	return new sendfile_plugin(srv, name);
}
