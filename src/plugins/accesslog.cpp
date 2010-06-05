/* <x0/plugins/accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpHeader.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cerrno>

/**
 * \ingroup plugins
 * \brief implements an accesslog log facility - in spirit of "combined" mode of apache's accesslog logs.
 */
class accesslog_plugin :
	public x0::HttpPlugin
{
private:
	x0::HttpServer::request_post_hook::connection c;

	struct context : public x0::ScopeValue
	{
		std::string filename_;
		int fd_;

		context() :
			filename_(),
			fd_(-1)
		{
		}

		~context()
		{
			close();
		}

		bool open(const std::string& filename)
		{
			if (fd_ >= 0)
				::close(fd_);

			fd_ = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
			if (fd_ < 0)
				return false;

			filename_ = filename;
			return true;
		}

		void write(const std::string message)
		{
			if (fd_ < 0)
				return;

			int rv = ::write(fd_, message.c_str(), message.size());

			if (rv < 0)
				DEBUG("Couldn't write to accesslog(%s): %s", filename_.c_str(), strerror(errno));
		}

		void close()
		{
			if (fd_ >= 0)
			{
				::close(fd_);
				fd_ = -1;
			}
		}

		void reopen()
		{
			close();
			open(filename_);
		}

		virtual void merge(const x0::ScopeValue *value)
		{
			if (auto cx = dynamic_cast<const context *>(value))
			{
				if (filename_.empty())
				{
					filename_ = cx->filename_;
					reopen();
				}
			}
		}
	};

public:
	accesslog_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		using namespace std::placeholders;
		c = srv.request_done.connect(std::bind(&accesslog_plugin::request_done, this, _1, _2));

		declareCVar("AccessLog", x0::HttpContext::server | x0::HttpContext::host, &accesslog_plugin::setup_log);
	}

	~accesslog_plugin()
	{
		server_.request_done.disconnect(c);
	}

private:
	bool setup_log(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		std::string filename;

		if (cvar.load(filename))
		{
			auto cx = s.acquire<context>(this);
			return cx->open(filename);
		}
		else
		{
			return false;
		}
	}

	void request_done(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (auto stream = server_.host(in->hostid()).get<context>(this))
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

	inline context *getlogstream(x0::HttpRequest *in)
	{
		return server_.host(in->hostid()).get<context>(this);
	}

	inline std::string hostname(x0::HttpRequest *in)
	{
		std::string name = in->connection.remote_ip();
		return !name.empty() ? name : "-";
	}

	inline std::string username(x0::HttpRequest *in)
	{
		return !in->username.empty() ? in->username.str() : "-";
	}

	inline std::string request_line(x0::HttpRequest *in)
	{
		std::stringstream str;

		str << in->method.str() << ' ' << in->uri.str()
			<< " HTTP/" << in->http_version_major << '.' << in->http_version_minor;

		return str.str();
	}

	inline std::string getheader(const x0::HttpRequest *in, const std::string& name)
	{
		x0::BufferRef value(in->header(name));
		return !value.empty() ? value.str() : "-";
	}
};

X0_EXPORT_PLUGIN(accesslog);
