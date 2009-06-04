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
	x0::handler::connection c;

public:
	sendfile_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.generate_content.connect(boost::bind(&sendfile_plugin::sendfile, this, _1, _2));
	}

	~sendfile_plugin() {
		server_.generate_content.disconnect(c);
	}

	virtual void configure()
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

		std::string input(x0::read_file(server_.get_config().get("sendfile", "mime-types")));
		tokenizer lines(input, boost::char_separator<char>("\n"));

		for (tokenizer::iterator i = lines.begin(), e = lines.end(); i != e; ++i)
		{
			tokenizer columns(x0::trim(*i), boost::char_separator<char>(" \t"));

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
			// TODO log errno
			return false;
		}

		// XXX setup some response headers
		set_content_type(in, out);

		out.content.resize(st.st_size);
		::read(fd, &out.content[0], st.st_size);

		// XXX send out headers, as they're fixed size in user space.
		// XXX start async transfer through sendfile()

		return true;
	}

	inline void set_content_type(x0::request& in, x0::response& out)
	{
		std::size_t ndot = in.entity.find_last_of(".");
		std::size_t nslash = in.entity.find_last_of("/");

		if (ndot != std::string::npos && ndot > nslash)
		{
			std::string extension = in.entity.substr(ndot + 1);
			out += x0::header("Content-Type", get_mime_type(extension));
		}
		else
		{
			out += x0::header("Content-Type", "text/plain");
		}
	}

	inline std::string get_mime_type(const std::string& ext) const
	{
		mime_types_type::const_iterator i = mime_types_.find(ext);
		return i != mime_types_.end() ? i->second : "text/plain";
	}
};

extern "C" x0::plugin *sendfile_init(x0::server& srv, const std::string& name) {
	return new sendfile_plugin(srv, name);
}
