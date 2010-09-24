/* <x0/plugins/accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
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
	x0::HttpServer::RequestPostHook::Connection c;

	struct LogFile : public x0::CustomData
	{
		std::string filename_;
		int fd_;

		LogFile() :
			filename_(),
			fd_(-1)
		{
			printf("LogFile()\n");
		}

		~LogFile()
		{
			printf("~LogFile()\n");
			close();
		}

		std::error_code open(const std::string& filename)
		{
			if (fd_ >= 0)
				::close(fd_);

			fd_ = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644);
			if (fd_ < 0)
				return std::make_error_code(static_cast<std::errc>(errno));

			filename_ = filename;
			return std::error_code();
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
			printf("closing\n");
		}

		void reopen()
		{
			close();
			open(filename_);
		}
	};

public:
	accesslog_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		c = srv.onRequestDone.connect<accesslog_plugin, &accesslog_plugin::request_done>(this);
	}

	~accesslog_plugin()
	{
		server_.onRequestDone.disconnect(c);
	}

private:
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		std::shared_ptr<LogFile> lf(std::make_shared<LogFile>());
		std::error_code ec = lf->open(args[0].toString());
		if (!ec)
			in->custom_data[this] = lf;
		else
			printf("accesslog error: %s\n", ec.message().c_str());

		return false;
	}

	void request_done(x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (LogFile *stream = (LogFile *)in->custom_data[this].get())
		{
			std::stringstream sstr;
			sstr << hostname(in);
			sstr << " - "; // identity as of identd
			sstr << username(in) << ' ';
			sstr << server_.now().htlog_str().c_str() << " \"";
			sstr << request_line(in) << "\" ";
			sstr << static_cast<int>(out->status) << ' ';
			sstr << out->headers["Content-Length"] << ' ';
			sstr << '"' << getheader(in, "Referer") << "\" ";
			sstr << '"' << getheader(in, "User-Agent") << '"';
			sstr << std::endl;

			stream->write(sstr.str());
		}
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
