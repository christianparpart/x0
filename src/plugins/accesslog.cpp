/* <x0/mod_accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/plugin.hpp>
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
 * \ingroup plugins
 * \brief implements an accesslog log facility - in spirit of "combined" mode of apache's accesslog logs.
 */
class accesslog_plugin :
	public x0::plugin
{
private:
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

	struct context
	{
		logstream *stream;

		context() : stream(0) {}
		context(const context& v) : stream(v.stream) {}
		~context() {}
	};

	logstream *getlogstream(const std::string& filename)
	{
		auto i = streams_.find(filename);

		if (i != streams_.end())
			return i->second.get();

		return (streams_[filename] = std::shared_ptr<logstream>(new logstream(filename))).get();
	}

	logstream *getlogstream(x0::request *in)
	{
		// vhost context
		if (context *ctx = server_.context<context>(this, in->hostid()))
			return ctx->stream;

		// server context
		if (context *ctx = server_.context<context>(this))
			return ctx->stream;

		return 0;
	}

private:
	x0::server::request_post_hook::connection c;
	std::map<std::string, std::shared_ptr<logstream>> streams_;

public:
	accesslog_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		streams_()
	{
		c = srv.request_done.connect(boost::bind(&accesslog_plugin::request_done, this, _1, _2));
	}

	~accesslog_plugin()
	{
		server_.request_done.disconnect(c);
	}

	virtual void configure()
	{
		std::string default_filename;
		bool has_global = server_.config().load("AccessLog", default_filename);

		if (has_global)
		{
			context *ctx = server_.create_context<context>(this);
			ctx->stream = getlogstream(default_filename);
		}

		auto hosts = server_.config()["Hosts"].keys<std::string>();
		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i)
		{
			std::string hostid(*i);
			context *ctx = server_.create_context<context>(this, hostid);

			std::string filename;
			if (server_.config()["Hosts"][hostid]["AccessLog"].load(filename))
			{
				ctx->stream = getlogstream(filename);
			}
			else if (has_global)
			{
				ctx->stream = getlogstream(default_filename);
			}
		}
	}

private:
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
