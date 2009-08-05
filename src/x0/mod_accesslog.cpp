/* <x0/mod_accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <cstring>
#include <cerrno>

/**
 * \ingroup modules
 * \brief implements an accesslog log facility - in spirit of "combined" mode of apache's accesslog logs.
 */
class accesslog_plugin :
	public x0::plugin
{
private:
	boost::signals::connection c;
	std::string filename;
	int fd;

public:
	accesslog_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		filename(), fd(-1)
	{
		c = srv.request_done.connect(boost::bind(&accesslog_plugin::request_done, this, _1, _2));
	}

	~accesslog_plugin()
	{
		server_.request_done.disconnect(c);

		if (fd != -1)
		{
			::close(fd);
		}
	}

	virtual void configure()
	{
		// TODO retrieve file to store accesslog log to.
		filename = server_.config().get("service", "accesslog-filename");

		if (!filename.empty())
		{
			fd = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
			if (fd == -1)
			{
				server_.log(x0::severity::error, "Could not open access log file.");
			}
		}
	}

private:
	void request_done(x0::request& in, x0::response& out)
	{
		if (fd != -1)
		{
			std::stringstream sstr;
			sstr << hostname(in);
			sstr << " - "; // identity as of identd
			sstr << username(in) << ' ';
			sstr << now() << " \"";
			sstr << request_line(in) << "\" ";
			sstr << out.status << ' ';
			sstr << out.content_length() << ' ';
			sstr << '"' << getheader(in, "Referer") << "\" ";
			sstr << '"' << getheader(in, "User-Agent") << '"';
			sstr << std::endl;

			std::string line(sstr.str());

			if (::write(fd, line.c_str(), line.size()) == -1)
				DEBUG("Couldn't write accesslog: %s", strerror(errno));
		}
	}

	inline std::string hostname(x0::request& in)
	{
		std::string name = in.connection.socket().remote_endpoint().address().to_string();
		return !name.empty() ? name : "-";
	}

	inline std::string username(x0::request& in)
	{
		return !in.username.empty() ? in.username : "-";
	}

	inline std::string request_line(x0::request& in)
	{
		std::stringstream str;

		str << in.method << ' ' << in.uri
			<< " HTTP/" << in.http_version_major << '.' << in.http_version_minor;

		return str.str();
	}

	inline std::string now()
	{
		char buf[26];
		std::time_t ts = time(0);

		if (struct tm *tm = localtime(&ts))
		{
			if (strftime(buf, sizeof(buf), "[%m/%d/%Y:%T %z]", tm) != 0)
			{
				return buf;
			}
		}
		return "-";
	}

	inline std::string getheader(const x0::request& in, const std::string& name)
	{
		std::string value(in.header(name));
		return !value.empty() ? value : "-";
	}
};

X0_EXPORT_PLUGIN(accesslog);
