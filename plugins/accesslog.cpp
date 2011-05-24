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

#include <unordered_map>
#include <string>
#include <cerrno>

/**
 * \ingroup plugins
 * \brief implements an accesslog log facility - in spirit of "combined" mode of apache's accesslog logs.
 */
class AccesslogPlugin :
	public x0::HttpPlugin
{
private:
	std::unordered_map<std::string, int> logfiles_; // map of file's name-to-fd

	struct RequestLogger // {{{
		: public x0::CustomData
	{
		int fd_;
		x0::HttpRequest *in_;

		RequestLogger(int fd, x0::HttpRequest *in) :
			fd_(fd), in_(in)
		{
		}

		~RequestLogger()
		{
			x0::Buffer sstr;
			sstr << hostname(in_);
			sstr << " - "; // identity as of identd
			sstr << username(in_) << ' ';
			sstr << in_->connection.worker().now().htlog_str().c_str() << " \"";
			sstr << request_line(in_) << "\" ";
			sstr << static_cast<int>(in_->status) << ' ';
			sstr << in_->bytesTransmitted() << ' ';
			sstr << '"' << getheader(in_, "Referer") << "\" ";
			sstr << '"' << getheader(in_, "User-Agent") << '"';
			sstr << '\n';

			::write(fd_, sstr.data(), sstr.size());
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
	AccesslogPlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerProperty<AccesslogPlugin, &AccesslogPlugin::handleRequest>("accesslog", x0::FlowValue::VOID);
	}

	~AccesslogPlugin()
	{
		clear();
	}

	void clear()
	{
		for (auto& i: logfiles_)
			::close(i.second);

		logfiles_.clear();
	}

private:
	void handleRequest(x0::FlowValue& result, x0::HttpRequest *in, const x0::FlowParams& args)
	{
		std::string filename(args[0].toString());
		auto i = logfiles_.find(filename);
		if (i != logfiles_.end()) {
			if (i->second >= 0) {
				in->setCustomData<RequestLogger>(this, i->second, in);
			}
		} else {
			int fd = ::open(filename.c_str(), O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE | O_CLOEXEC, 0644);
			if (fd >= 0) {
				logfiles_[filename] = fd;
				in->setCustomData<RequestLogger>(this, fd, in);
			} else {
				in->log(x0::Severity::error, "Could not open accesslog file (%s): %s",
						filename.c_str(), strerror(errno));
			}
		}
	}
};

X0_EXPORT_PLUGIN_CLASS(AccesslogPlugin)
