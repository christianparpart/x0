/* <x0/plugins/accesslog.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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
 *     void accesslog.syslog();
 */

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/SyslogSink.h>
#include <x0/LogFile.h>
#include <x0/Logger.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(HAVE_SYSLOG_H)
#	include <syslog.h>
#endif

#include <unordered_map>
#include <string>
#include <cerrno>

/**
 * \ingroup plugins
 * \brief implements an accesslog log facility - in spirit of "combined" mode of apache's accesslog logs.
 */
class AccesslogPlugin :
	public x0d::XzeroPlugin
{
private:
	typedef std::unordered_map<std::string, std::shared_ptr<x0::LogFile>> LogMap;

#if defined(HAVE_SYSLOG_H)
	x0::SyslogSink syslogSink_;
#endif

	LogMap logfiles_; // map of file's name-to-fd

	struct RequestLogger // {{{
		: public x0::CustomData
	{
		x0::Sink* log_;
		x0::HttpRequest *in_;

		RequestLogger(x0::Sink* log, x0::HttpRequest *in) :
			log_(log), in_(in)
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

			if (log_->write(sstr.c_str(), sstr.size()) < static_cast<ssize_t>(sstr.size())) {
				in_->log(x0::Severity::error, "Could not write to accesslog target. %s", strerror(errno));
			}
		}

		inline std::string hostname(x0::HttpRequest *in)
		{
			std::string name = in->connection.remoteIP().str();
			return !name.empty() ? name : "-";
		}

		inline std::string username(x0::HttpRequest *in)
		{
			return !in->username.empty() ? in->username : "-";
		}

		inline std::string request_line(x0::HttpRequest *in)
		{
			x0::Buffer buf;

			buf << in->method << ' ' << in->unparsedUri
				<< " HTTP/" << in->httpVersionMajor << '.' << in->httpVersionMinor;

			return buf.str();
		}

		inline std::string getheader(const x0::HttpRequest *in, const std::string& name)
		{
			x0::BufferRef value(in->requestHeader(name));
			return !value.empty() ? value.str() : "-";
		}
	}; // }}}

public:
	AccesslogPlugin(x0d::XzeroDaemon* d, const std::string& name) :
		x0d::XzeroPlugin(d, name),
#if defined(HAVE_SYSLOG_H)
		syslogSink_(LOG_INFO),
#endif
		logfiles_()
	{
		mainFunction("accesslog", &AccesslogPlugin::handleRequest, x0::FlowType::String);
		mainFunction("accesslog.syslog", &AccesslogPlugin::syslogHandler);
	}

	~AccesslogPlugin()
	{
		clear();
	}

	virtual void cycleLogs()
	{
		for (auto& i: logfiles_) {
			i.second->cycle();
		}
	}

	void clear()
	{
		logfiles_.clear();
	}

private:
	void syslogHandler(x0::HttpRequest *in, x0::FlowVM::Params& args)
	{
#if defined(HAVE_SYSLOG_H)
		in->setCustomData<RequestLogger>(this, &syslogSink_, in);
#endif
	}

	void handleRequest(x0::HttpRequest *in, x0::FlowVM::Params& args)
	{
		std::string filename(args.getString(1).str());
		auto i = logfiles_.find(filename);
		if (i != logfiles_.end()) {
			if (i->second.get()) {
				in->setCustomData<RequestLogger>(this, i->second.get(), in);
			}
		} else {
			auto fileSink = new x0::LogFile(filename);
			logfiles_[filename].reset(fileSink);
			in->setCustomData<RequestLogger>(this, fileSink, in);
		}
	}
};

X0_EXPORT_PLUGIN_CLASS(AccesslogPlugin)
