/* <x0/plugins/accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: logger
 *
 * description:
 *     Logs incoming requests to a local file.
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     void accesslog(string logfilename);
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
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
	x0::HttpServer::RequestHook::Connection c;

	struct RequestLogger : public x0::CustomData // {{{
	{
		std::string filename_;
		x0::HttpRequest *in_;

		RequestLogger(const std::string& filename, x0::HttpRequest *in) :
			filename_(filename), in_(in)
		{
		}

		~RequestLogger()
		{
			int fd = open(filename_.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE | O_CLOEXEC, 0644);
			if (fd < 0)
				return;

			std::stringstream sstr;
			sstr << hostname(in_);
			sstr << " - "; // identity as of identd
			sstr << username(in_) << ' ';
			sstr << in_->connection.worker().now().htlog_str().c_str() << " \"";
			sstr << request_line(in_) << "\" ";
			sstr << static_cast<int>(in_->status) << ' ';
			sstr << in_->responseHeaders["Content-Length"] << ' ';
			sstr << '"' << getheader(in_, "Referer") << "\" ";
			sstr << '"' << getheader(in_, "User-Agent") << '"';
			sstr << std::endl;

			std::string out = sstr.str();
			::write(fd, out.data(), out.size());
			::close(fd);
		}

		inline std::string hostname(x0::HttpRequest *in)
		{
			std::string name = in->connection.remoteIP();
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
				<< " HTTP/" << in->httpVersionMajor << '.' << in->httpVersionMinor;

			return str.str();
		}

		inline std::string getheader(const x0::HttpRequest *in, const std::string& name)
		{
			x0::BufferRef value(in->requestHeader(name));
			return !value.empty() ? value.str() : "-";
		}
	}; // }}}

public:
	accesslog_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerProperty<accesslog_plugin, &accesslog_plugin::handleRequest>("accesslog", Flow::Value::VOID);
	}

	~accesslog_plugin()
	{
		server_.onRequestDone.disconnect(c);
	}

private:
	void handleRequest(Flow::Value& result, x0::HttpRequest *in, const x0::Params& args)
	{
		in->setCustomData<RequestLogger>(this, args[0].toString(), in);
	}
};

X0_EXPORT_PLUGIN(accesslog)
