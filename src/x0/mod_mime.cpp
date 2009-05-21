/* <x0/mod_mime.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>

/**
 * \ingroup modules
 * \brief Automatically assigns a Content-Type response header based on request URI's file extension.
 */
class mime_plugin :
	public x0::plugin
{
private:
	boost::signals::connection c;
	std::map<std::string, std::string> mime_types_;

public:
	mime_plugin(x0::server& srv) :
		x0::plugin(srv)
	{
		c = srv.response_header_generator.connect(boost::bind(&mime_plugin::response_header_generator, this, _1, _2));
	}

	~mime_plugin()
	{
		server_.response_header_generator.disconnect(c);
	}

	virtual void configure()
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

		std::string input(x0::read_file("/etc/mime.types"));
		tokenizer lines(input, boost::char_separator<char>("\n"));

		for (auto i = lines.begin(), e = lines.end(); i != e; ++i)
		{
			tokenizer columns(x0::trim(*i), boost::char_separator<char>(" \t"));

			auto ci = columns.begin(), ce = columns.end();
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

	std::string get_mime_type(const std::string& ext) const
	{
		auto i = mime_types_.find(ext);
		return i != mime_types_.end() ? i->second : "text/plain";
	}

private:
	void response_header_generator(x0::request& in, x0::response& out)
	{
		if (!out.has_header("Content-Type"))
		{
			std::size_t ndot = in.path.find_last_of(".");
			std::size_t nslash = in.path.find_last_of("/");

			if (ndot != std::string::npos && ndot > nslash)
			{
				std::string extension = in.path.substr(ndot + 1);
				out += x0::header("Content-Type", get_mime_type(extension));
			}
			else
			{
				out += x0::header("Content-Type", "text/plain");
			}
		}
	}
};

extern "C" void mime_init(x0::server& srv) {
	srv.setup_plugin(x0::plugin_ptr(new mime_plugin(srv)));
}
