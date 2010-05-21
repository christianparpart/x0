/* <x0/mod_accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/plugin.hpp>
#include <x0/http/server.hpp>
#include <x0/http/request.hpp>
#include <x0/http/response.hpp>
#include <x0/http/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cerrno>

struct logstream
{
	std::string filename_;
	int fd_;

	explicit logstream(const std::string& filename)
	{
		filename_ = filename;
		fd_ = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
	}

	~logstream()
	{
		if (fd_)
			::close(fd_);
	}

	void write(const std::string message)
	{
		if (fd_ < 0)
			return;

		if (::write(fd_, message.c_str(), message.size()) < 0)
			DEBUG("Couldn't write accesslog(%s): %s", filename_.c_str(), strerror(errno));
	}
};

typedef std::shared_ptr<logstream> logstream_ptr;

/**
 * \ingroup plugins
 * \brief implements an accesslog log facility - in spirit of "combined" mode of apache's accesslog logs.
 */
class accesslog_plugin :
	public x0::plugin
{
private:
	x0::server::request_post_hook::connection c;

	//! maps file-name to log-stream (to manage only one stream per filename).
	std::map<std::string, logstream_ptr> streams_;

	//! maps host-id to log-stream.
	std::map<std::string, logstream_ptr> host_logs;

	//! global log-stream, used when no other applies.
	logstream_ptr global_log;

public:
	accesslog_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		streams_()
	{
		using namespace std::placeholders;
		c = srv.request_done.connect(std::bind(&accesslog_plugin::request_done, this, _1, _2));

		srv.register_cvar_host("AccessLog", std::bind(&accesslog_plugin::setup_per_host, this, _1, _2));
		srv.register_cvar_server("AccessLog", std::bind(&accesslog_plugin::setup_per_srv, this, _1));
	}

	~accesslog_plugin()
	{
		server_.request_done.disconnect(c);
	}

private:
	void setup_per_srv(const x0::settings_value& cvar)
	{
		global_log = getlogstream(cvar.as<std::string>());
	}

	void setup_per_host(const x0::settings_value& cvar, const std::string& hostid)
	{
		host_logs[hostid] = getlogstream(cvar.as<std::string>());
	}

	void request_done(x0::request *in, x0::response *out)
	{
		if (auto stream = getlogstream(in))
		{
			std::stringstream sstr;
			sstr << hostname(in);
			sstr << " - "; // identity as of identd
			sstr << username(in) << ' ';
			sstr << server_.now().htlog_str().c_str() << " \"";
			sstr << request_line(in) << "\" ";
			sstr << out->status << ' ';
			sstr << out->headers["Content-Length"] << ' ';
			sstr << '"' << getheader(in, "Referer") << "\" ";
			sstr << '"' << getheader(in, "User-Agent") << '"';
			sstr << std::endl;

			stream->write(sstr.str());
		}
	}

	inline logstream_ptr getlogstream(const std::string& filename)
	{
		auto i = streams_.find(filename);

		if (i != streams_.end())
			return i->second;

		return (streams_[filename] = std::shared_ptr<logstream>(new logstream(filename)));
	}

	inline logstream_ptr getlogstream(x0::request *in)
	{
		// vhost context
		auto i = host_logs.find(in->hostid());
		if (i != host_logs.end())
			return i->second;

		// server context
		if (auto i = global_log)
			return i;

		return logstream_ptr();
	}

	inline std::string hostname(x0::request *in)
	{
		std::string name = in->connection.remote_ip();
		return !name.empty() ? name : "-";
	}

	inline std::string username(x0::request *in)
	{
		return !in->username.empty() ? in->username.str() : "-";
	}

	inline std::string request_line(x0::request *in)
	{
		std::stringstream str;

		str << in->method.str() << ' ' << in->uri.str()
			<< " HTTP/" << in->http_version_major << '.' << in->http_version_minor;

		return str.str();
	}

	inline std::string getheader(const x0::request *in, const std::string& name)
	{
		x0::buffer_ref value(in->header(name));
		return !value.empty() ? value.str() : "-";
	}
};

X0_EXPORT_PLUGIN(accesslog);
