/* <x0/HttpCore.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpCore.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/http/HttpListener.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FileSource.h>
#include <x0/Types.h>
#include <x0/DateTime.h>
#include <x0/Logger.h>
#include <x0/strutils.h>

#include <boost/lexical_cast.hpp>
#include <sstream>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

namespace x0 {

HttpCore::HttpCore(HttpServer& server) :
	HttpPlugin(server, "core"),
	emitLLVM_(false),
	max_fds(std::bind(&HttpCore::getrlimit, this, RLIMIT_NOFILE),
			std::bind(&HttpCore::setrlimit, this, RLIMIT_NOFILE, std::placeholders::_1))
{
	// setup
	registerSetupFunction<HttpCore, &HttpCore::emit_llvm>("llvm.dump", Flow::Value::VOID);
	registerSetupFunction<HttpCore, &HttpCore::listen>("listen", Flow::Value::VOID);
	registerSetupFunction<HttpCore, &HttpCore::log_sd>("log.systemd", Flow::Value::VOID);
	registerSetupProperty<HttpCore, &HttpCore::loglevel>("log.level", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::logfile>("log.file", Flow::Value::STRING);
	registerSetupProperty<HttpCore, &HttpCore::workers>("workers", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::mimetypes>("mimetypes", Flow::Value::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::mimetypes_default>("mimetypes.default", Flow::Value::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::etag_mtime>("etag.mtime", Flow::Value::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::etag_size>("etag.size", Flow::Value::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::etag_inode>("etag.inode", Flow::Value::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::fileinfo_cache_ttl>("fileinfo.ttl", Flow::Value::VOID); // write-only (int)
	registerSetupProperty<HttpCore, &HttpCore::server_advertise>("server.advertise", Flow::Value::BOOLEAN);
	registerSetupProperty<HttpCore, &HttpCore::server_tags>("server.tags", Flow::Value::VOID); // write-only (array)

	registerSetupProperty<HttpCore, &HttpCore::max_read_idle>("max_read_idle", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_write_idle>("max_write_idle", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_keepalive_idle>("max_keepalive_idle", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_conns>("max_connections", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_files>("max_files", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_address_space>("max_address_space", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_core>("max_core_size", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::tcp_cork>("tcp_cork", Flow::Value::BOOLEAN);
	registerSetupProperty<HttpCore, &HttpCore::tcp_nodelay>("tcp_nodelay", Flow::Value::BOOLEAN);

	// TODO setup error-documents

	// shared
	registerSetupFunction<HttpCore, &HttpCore::sys_env>("sys.env", Flow::Value::STRING);
	registerSetupProperty<HttpCore, &HttpCore::sys_cwd>("sys.cwd", Flow::Value::STRING);
	registerSetupProperty<HttpCore, &HttpCore::sys_pid>("sys.pid", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::sys_now>("sys.now", Flow::Value::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::sys_now_str>("sys.now_str", Flow::Value::STRING);

	// main
	registerHandler<HttpCore, &HttpCore::docroot>("docroot");
	registerHandler<HttpCore, &HttpCore::alias>("alias");
	registerFunction<HttpCore, &HttpCore::autoindex>("autoindex", Flow::Value::VOID);
	registerFunction<HttpCore, &HttpCore::pathinfo>("pathinfo", Flow::Value::VOID);
	registerProperty<HttpCore, &HttpCore::req_method>("req.method", Flow::Value::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_url>("req.url", Flow::Value::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_path>("req.path", Flow::Value::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_header>("req.header", Flow::Value::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_host>("req.host", Flow::Value::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_pathinfo>("req.pathinfo", Flow::Value::STRING);
	registerProperty<HttpCore, &HttpCore::req_is_secure>("req.is_secure", Flow::Value::BOOLEAN);
	registerFunction<HttpCore, &HttpCore::resp_header_add>("header.add", Flow::Value::VOID);
	registerFunction<HttpCore, &HttpCore::resp_header_overwrite>("header.overwrite", Flow::Value::VOID);
	registerFunction<HttpCore, &HttpCore::resp_header_append>("header.append", Flow::Value::VOID);
	registerFunction<HttpCore, &HttpCore::resp_header_remove>("header.remove", Flow::Value::VOID);
	registerProperty<HttpCore, &HttpCore::conn_remote_ip>("req.remoteip", Flow::Value::STRING);
	registerProperty<HttpCore, &HttpCore::conn_remote_port>("req.remoteport", Flow::Value::NUMBER);
	registerProperty<HttpCore, &HttpCore::conn_local_ip>("req.localip", Flow::Value::STRING);
	registerProperty<HttpCore, &HttpCore::conn_local_port>("req.localport", Flow::Value::NUMBER);
	registerProperty<HttpCore, &HttpCore::phys_path>("phys.path", Flow::Value::STRING);
	registerProperty<HttpCore, &HttpCore::phys_exists>("phys.exists", Flow::Value::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_is_reg>("phys.is_reg", Flow::Value::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_is_dir>("phys.is_dir", Flow::Value::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_is_exe>("phys.is_exe", Flow::Value::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_mtime>("phys.mtime", Flow::Value::NUMBER);
	registerProperty<HttpCore, &HttpCore::phys_size>("phys.size", Flow::Value::NUMBER);
	registerProperty<HttpCore, &HttpCore::phys_etag>("phys.etag", Flow::Value::STRING);
	registerProperty<HttpCore, &HttpCore::phys_mimetype>("phys.mimetype", Flow::Value::STRING);

	// main handlers
	registerHandler<HttpCore, &HttpCore::staticfile>("staticfile");
	registerHandler<HttpCore, &HttpCore::redirect>("redirect");
	registerHandler<HttpCore, &HttpCore::respond>("respond");
}

HttpCore::~HttpCore()
{
}

// {{{ setup
void HttpCore::mimetypes(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isString())
	{
		server().fileinfoConfig_.loadMimetypes(args[0].toString());
	}
}

void HttpCore::mimetypes_default(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isString())
	{
		server().fileinfoConfig_.defaultMimetype = args[0].toString();
	}
}

void HttpCore::etag_mtime(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isBool())
		server().fileinfoConfig_.etagConsiderMtime = args[0].toBool();
	else
		result.set(server().fileinfoConfig_.etagConsiderMtime);
}

void HttpCore::etag_size(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isBool())
		server().fileinfoConfig_.etagConsiderSize = args[0].toBool();
	else
		result.set(server().fileinfoConfig_.etagConsiderSize);
}

void HttpCore::etag_inode(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isBool())
		server().fileinfoConfig_.etagConsiderInode = args[0].toBool();
	else
		result.set(server().fileinfoConfig_.etagConsiderInode);
}

void HttpCore::fileinfo_cache_ttl(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		server().fileinfoConfig_.cacheTTL_ = args[0].toNumber();
	else
		result.set(server().fileinfoConfig_.cacheTTL_);
}

void HttpCore::server_advertise(Flow::Value& result, const Params& args)
{
	if (args.empty())
	{
		result.set(server().advertise());
	}
	else // TODO if (args.expect(Flow::Value::NUMBER))
	{
		server().advertise(args[0].toBool());
	}
}

void HttpCore::server_tags(Flow::Value& result, const Params& args)
{
	for (size_t i = 0, e = args.count(); i != e; ++i)
		loadServerTag(args[i]);
}

void HttpCore::loadServerTag(const Flow::Value& tag)
{
	switch (tag.type())
	{
		case Flow::Value::ARRAY:
			for (const Flow::Value *a = tag.toArray(); !a->isVoid(); ++a)
				loadServerTag(*a);
			break;
		case Flow::Value::STRING:
			if (*tag.toString() != '\0')
				server().components_.push_back(tag.toString());
			break;
		case Flow::Value::BUFFER:
			if (tag.toNumber() > 0)
				server().components_.push_back(std::string(tag.toString(), tag.toNumber()));
			break;
		default:
			;//TODO reportWarning("Skip invalid value in server.tag: '%s'.", d->dump().c_str());
	}
}

void HttpCore::max_read_idle(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		server().max_read_idle(args[0].toNumber());
	else
		result.set(server().max_read_idle());
}

void HttpCore::max_write_idle(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		server().max_write_idle(args[0].toNumber());
	else
		result.set(server().max_write_idle());
}

void HttpCore::max_keepalive_idle(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		server().max_keep_alive_idle(args[0].toNumber());
	else
		result.set(server().max_keep_alive_idle());
}

void HttpCore::max_conns(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		server().max_connections(args[0].toNumber());
	else
		result.set(server().max_connections());
}

void HttpCore::max_files(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		setrlimit(RLIMIT_NOFILE, args[0].toNumber());
	else
		result.set(getrlimit(RLIMIT_NOFILE));
}

void HttpCore::max_address_space(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		setrlimit(RLIMIT_AS, args[0].toNumber());
	else
		result.set(getrlimit(RLIMIT_AS));
}

void HttpCore::max_core(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isNumber())
		setrlimit(RLIMIT_CORE, args[0].toNumber());
	else
		result.set(getrlimit(RLIMIT_CORE));
}

void HttpCore::tcp_cork(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isBool())
		server().tcp_cork(args[0].toBool());
	else
		result.set(server().tcp_cork());
}

void HttpCore::tcp_nodelay(Flow::Value& result, const Params& args)
{
	if (args.count() == 1 && args[0].isBool())
		server().tcp_nodelay(args[0].toBool());
	else
		result.set(server().tcp_nodelay());
}

void HttpCore::listen(Flow::Value& result, const Params& args)
{
	std::string arg(args[0].toString());
	size_t n = arg.rfind(':');
	std::string ip = n != std::string::npos ? arg.substr(0, n) : "0.0.0.0";
	int port = atoi(n != std::string::npos ? arg.substr(n + 1).c_str() : arg.c_str());
	int backlog = args[1].isNumber() ? args[1].toNumber() : 0;

	HttpListener *listener = server().setupListener(port, ip);

	if (listener && backlog)
		listener->backlog(backlog);

	result.set(listener == nullptr);
}

void HttpCore::workers(Flow::Value& result, const Params& args)
{
	if (args.count() == 1) {
		if (args[0].isArray()) {
			size_t i = 0;
			size_t count = server_.workers_.size();

			// spawn or set affinity of a set of workers as passed via input array
			for (const Flow::Value *value = args[0].toArray(); !value->isVoid(); ++value) {
				if (value->isNumber()) {
					if (i >= count)
						server_.spawnWorker();

					server_.workers_[i]->setAffinity(value->toNumber());
					++i;
				}
			}

			// destroy workers that exceed our input array
			for (count = server_.workers_.size(); i < count; --count) {
				server_.destroyWorker(server_.workers_[count - 1]);
			}
		} else {
			size_t cur = server_.workers_.size();
			size_t count = args.count() == 1 ? args[0].toNumber() : 1;

			for (; cur < count; ++cur) {
				server_.spawnWorker();
			}

			for (; cur > count; --cur) {
				server_.destroyWorker(server_.workers_[cur - 1]);
			}
		}
	}

	result.set(server_.workers_.size());
}

extern std::string global_now(); // HttpServer.cpp

void HttpCore::logfile(Flow::Value& result, const Params& args)
{
	if (args.count() == 1)
	{
		if (args[0].isString())
		{
			const char *filename = args[0].toString();
			auto nowfn = std::bind(&global_now);
			server_.logger_.reset(new FileLogger<decltype(nowfn)>(filename, nowfn));
		}
	}
}

void HttpCore::log_sd(Flow::Value& result, const Params& args)
{
	if (args.count() == 0)
	{
		server_.logger_.reset(new SystemdLogger());
	}
}

void HttpCore::loglevel(Flow::Value& result, const Params& args)
{
	if (args.count() == 0)
	{
		result.set(server().logLevel());
	}
	else if (args.count() == 1)
	{
		if (args[0].isNumber())
		{
			int level = args[0].toNumber();
			server().logLevel(static_cast<Severity>(level));
		}
	}
}

void HttpCore::emit_llvm(Flow::Value& result, const Params& args)
{
	emitLLVM_ = true;
}
// }}}

// {{{ sys
void HttpCore::sys_env(Flow::Value& result, const Params& args)
{
	result.set(getenv(args[0].toString()));
}

void HttpCore::sys_cwd(Flow::Value& result, const Params& args)
{
	static char buf[1024];
	result.set(getcwd(buf, sizeof(buf)));
}

void HttpCore::sys_pid(Flow::Value& result, const Params& args)
{
	result.set(getpid());
}

void HttpCore::sys_now(Flow::Value& result, const Params& args)
{
	result.set(static_cast<uint64_t>(server().workers_[0]->now_.unixtime()));
}

void HttpCore::sys_now_str(Flow::Value& result, const Params& args)
{
	auto& s = server().workers_[0]->now_.http_str();
	result.set(s.data(), s.size());
}
// }}}

// {{{ req
void HttpCore::autoindex(Flow::Value& result, HttpRequest *in, const Params& args)
{
	if (in->documentRoot.empty()) {
		server().log(Severity::error, "autoindex: No document root set yet. Skipping.");
		return; // error: must have a document-root set first.
	}

	if (!in->fileinfo)
		return; // something went wrong, just be sure we SEGFAULT here

	if (!in->fileinfo->isDirectory())
		return;

	if (args.count() < 1)
		return;

	for (size_t i = 0, e = args.count(); i != e; ++i)
		if (matchIndex(in, args[i]))
			return;
}

bool HttpCore::matchIndex(HttpRequest *in, const Flow::Value& arg)
{
	std::string path(in->fileinfo->filename());

	switch (arg.type())
	{
		case Flow::Value::STRING:
		{
			std::string ipath;
			ipath.reserve(path.length() + 1 + strlen(arg.toString()));
			ipath += path;
			if (path[path.size() - 1] != '/')
				ipath += "/";
			ipath += arg.toString();

			if (x0::FileInfoPtr fi = in->connection.worker().fileinfo(ipath))
			{
				if (fi->isRegular())
				{
					in->fileinfo = fi;
					return true;
				}
			}
			break;
		}
		case Flow::Value::ARRAY:
		{
			for (const Flow::Value *a = arg.toArray(); !a->isVoid(); ++a)
				if (matchIndex(in, *a))
					return true;

			break;
		}
		default:
			break;
	}
	return false;
}

bool HttpCore::docroot(HttpRequest *in, const Params& args)
{
	if (args.count() != 1)
		return false;

	in->documentRoot = args[0].toString();
	in->fileinfo = in->connection.worker().fileinfo(in->documentRoot + in->path);
	// XXX; we could autoindex here in case the user told us an autoindex before the docroot.

	return redirectOnIncompletePath(in);
}

bool HttpCore::alias(HttpRequest *in, const Params& args)
{
	if (args.count() != 1 || !args[0].isArray())
	{
		server().log(Severity::error, "alias: invalid argument count");
		return false;
	}

	if (!args[0][0].isString() || !args[0][1].isString())
	{
		server().log(Severity::error, "alias: invalid argument types");
		return false;
	}

	// input:
	//    URI: /some/uri/path
	//    Alias '/some' => '/srv/special';
	//
	// output:
	//    docroot: /srv/special
	//    fileinfo: /srv/special/uri/path

	size_t prefixLength = strlen(args[0][0].toString());
	std::string prefix = args[0][0].toString();
	std::string alias = args[0][1].toString();

	if (in->path.begins(prefix))
		in->fileinfo = in->connection.worker().fileinfo(alias + in->path.substr(prefixLength));

	return redirectOnIncompletePath(in);
}

void HttpCore::pathinfo(Flow::Value& result, HttpRequest *in, const Params& args)
{
	if (!in->fileinfo) {
		in->log(Severity::error,
				"pathinfo: no file information available. Please set document root first.");
		return;
	}

	in->updatePathInfo();
}

void HttpCore::req_method(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->method.data(), in->method.size());
}

void HttpCore::req_url(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->uri.data(), in->uri.size());
}

void HttpCore::req_path(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->path.data(), in->path.size());
}

void HttpCore::req_header(Flow::Value& result, HttpRequest *in, const Params& args)
{
	BufferRef ref(in->requestHeader(args[0].toString()));
	result.set(ref.data(), ref.size());
}

void HttpCore::req_host(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->hostname.data(), in->hostname.size());
}

void HttpCore::req_pathinfo(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result = in->pathinfo.c_str();
}

void HttpCore::req_is_secure(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result = in->connection.isSecure();
}
// }}}

// {{{ response
void HttpCore::resp_header_add(Flow::Value& result, HttpRequest *in, const Params& args)
{
}

void HttpCore::resp_header_overwrite(Flow::Value& result, HttpRequest *in, const Params& args)
{
}

void HttpCore::resp_header_append(Flow::Value& result, HttpRequest *in, const Params& args)
{
}

void HttpCore::resp_header_remove(Flow::Value& result, HttpRequest *in, const Params& args)
{
}
// }}}

// {{{ connection
void HttpCore::conn_remote_ip(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result = in->connection.remoteIP().c_str();
}

void HttpCore::conn_remote_port(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result = in->connection.remotePort();
}

void HttpCore::conn_local_ip(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result = in->connection.localIP().c_str();
}

void HttpCore::conn_local_port(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result = in->connection.localPort();
}
// }}}

// {{{ phys
void HttpCore::phys_path(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->filename().c_str() : "");
}

void HttpCore::phys_exists(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->exists() : false);
}

void HttpCore::phys_is_reg(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->isRegular() : false);
}

void HttpCore::phys_is_dir(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->isDirectory() : false);
}

void HttpCore::phys_is_exe(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->isExecutable() : false);
}

void HttpCore::phys_mtime(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(static_cast<uint64_t>(in->fileinfo ? in->fileinfo->mtime() : 0));
}

void HttpCore::phys_size(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->size() : 0);
}

void HttpCore::phys_etag(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->etag().c_str() : "");
}

void HttpCore::phys_mimetype(Flow::Value& result, HttpRequest *in, const Params& args)
{
	result.set(in->fileinfo ? in->fileinfo->mimetype().c_str() : "");
}
// }}}

// {{{ handler
bool HttpCore::redirect(HttpRequest *in, const Params& args)
{
	in->status = HttpError::MovedTemporarily;
	in->responseHeaders.overwrite("Location", args[0].toString());
	in->finish();

	return true;
}

bool HttpCore::respond(HttpRequest *in, const Params& args)
{
	if (args.count() >= 1 && args[0].isNumber())
		in->status = static_cast<HttpError>(args[0].toNumber());

	in->finish();
	return true;
}
// }}}

// {{{ staticfile handler
bool HttpCore::staticfile(HttpRequest *in, const Params& args) // {{{
{
	if (!in->fileinfo) {
		in->log(Severity::error, "in->fileinfo not set");
		return false;
	}

	if (!in->fileinfo->exists())
		return false;

	if (!in->fileinfo->isRegular())
		return false;

	in->status = verifyClientCache(in);
	if (in->status != HttpError::Ok)
	{
		in->finish();
		return true;
	}

	int fd = -1;
	if (equals(in->method, "GET"))
	{
		int flags = O_RDONLY;

#if defined(O_NONBLOCK)
		flags |= O_NONBLOCK;
#endif
#if 0 // defined(O_CLOEXEC)
		flags |= O_CLOEXEC;
#endif

		fd = in->fileinfo->open(flags);

		if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC) < 0)
			log(Severity::error, "Could not set FD_CLOEXEC on %s: %s", in->fileinfo->filename().c_str(), strerror(errno));

		if (fd < 0)
		{
			server_.log(Severity::error, "Could not open file '%s': %s",
				in->fileinfo->filename().c_str(), strerror(errno));

			in->status = HttpError::Forbidden;
			in->finish();

			return true;
		}
	}
	else if (!equals(in->method, "HEAD"))
	{
		in->status = HttpError::MethodNotAllowed;
		in->finish();

		return true;
	}

	in->responseHeaders.push_back("Last-Modified", in->fileinfo->lastModified());
	in->responseHeaders.push_back("ETag", in->fileinfo->etag());

	if (!processRangeRequest(in, fd))
	{
		in->responseHeaders.push_back("Accept-Ranges", "bytes");
		in->responseHeaders.push_back("Content-Type", in->fileinfo->mimetype());
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(in->fileinfo->size()));

		if (fd < 0) // HEAD request
		{
			in->finish();
		}
		else
		{
			posix_fadvise(fd, 0, in->fileinfo->size(), POSIX_FADV_SEQUENTIAL);

			in->write(
				std::make_shared<FileSource>(fd, 0, in->fileinfo->size(), true),
				std::bind(&HttpRequest::finish, in)
			);
		}
	}
	return true;
} // }}}

/**
 * verifies wether the client may use its cache or not.
 *
 * \param in request object
 */
HttpError HttpCore::verifyClientCache(HttpRequest *in) // {{{
{
	std::string value;

	// If-None-Match, If-Modified-Since
	if ((value = in->requestHeader("If-None-Match")) != "")
	{
		if (value == in->fileinfo->etag())
		{
			if ((value = in->requestHeader("If-Modified-Since")) != "") // ETag + If-Modified-Since
			{
				DateTime date(value);

				if (!date.valid())
					return HttpError::BadRequest;

				if (in->fileinfo->mtime() <= date.unixtime())
					return HttpError::NotModified;
			}
			else // ETag-only
			{
				return HttpError::NotModified;
			}
		}
	}
	else if ((value = in->requestHeader("If-Modified-Since")) != "")
	{
		DateTime date(value);
		if (!date.valid())
			return HttpError::BadRequest;

		if (in->fileinfo->mtime() <= date.unixtime())
			return HttpError::NotModified;
	}

	return HttpError::Ok;
} // }}}

inline bool HttpCore::processRangeRequest(HttpRequest *in, int fd) //{{{
{
	BufferRef range_value(in->requestHeader("Range"));
	HttpRangeDef range;

	// if no range request or range request was invalid (by syntax) we fall back to a full response
	if (range_value.empty() || !range.parse(range_value))
		return false;

	in->status = HttpError::PartialContent;

	if (range.size() > 1)
	{
		// generate a multipart/byteranged response, as we've more than one range to serve

		auto content = std::make_shared<CompositeSource>();
		Buffer buf;
		std::string boundary(generateBoundaryID());
		std::size_t content_length = 0;

		for (int i = 0, e = range.size(); i != e; ++i)
		{
			std::pair<std::size_t, std::size_t> offsets(makeOffsets(range[i], in->fileinfo->size()));
			if (offsets.second < offsets.first)
			{
				in->status = HttpError::RequestedRangeNotSatisfiable;
				return true;
			}

			std::size_t length = 1 + offsets.second - offsets.first;

			buf.clear();
			buf.push_back("\r\n--");
			buf.push_back(boundary);
			buf.push_back("\r\nContent-Type: ");
			buf.push_back(in->fileinfo->mimetype());

			buf.push_back("\r\nContent-Range: bytes ");
			buf.push_back(boost::lexical_cast<std::string>(offsets.first));
			buf.push_back("-");
			buf.push_back(boost::lexical_cast<std::string>(offsets.second));
			buf.push_back("/");
			buf.push_back(boost::lexical_cast<std::string>(in->fileinfo->size()));
			buf.push_back("\r\n\r\n");

			if (fd >= 0)
			{
				bool lastChunk = i + 1 == e;
				content->push_back(std::make_shared<BufferSource>(std::move(buf)));
				content->push_back(std::make_shared<FileSource>(fd, offsets.first, length, lastChunk));
			}
			content_length += buf.size() + length;
		}

		buf.clear();
		buf.push_back("\r\n--");
		buf.push_back(boundary);
		buf.push_back("--\r\n");

		content->push_back(std::make_shared<BufferSource>(std::move(buf)));
		content_length += buf.size();

		in->responseHeaders.push_back("Content-Type", "multipart/byteranges; boundary=" + boundary);
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(content_length));

		if (fd >= 0)
		{
			in->write(content, std::bind(&HttpRequest::finish, in));
		}
		else
		{
			in->finish();
		}
	}
	else // generate a simple (single) partial response
	{
		std::pair<std::size_t, std::size_t> offsets(makeOffsets(range[0], in->fileinfo->size()));
		if (offsets.second < offsets.first)
		{
			in->status = HttpError::RequestedRangeNotSatisfiable;
			return true;
		}

		std::size_t length = 1 + offsets.second - offsets.first;

		in->responseHeaders.push_back("Content-Type", in->fileinfo->mimetype());
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(length));

		std::stringstream cr;
		cr << "bytes " << offsets.first << '-' << offsets.second << '/' << in->fileinfo->size();
		in->responseHeaders.push_back("Content-Range", cr.str());

		if (fd >= 0)
		{
			in->write(
				std::make_shared<FileSource>(fd, offsets.first, length, true),
				std::bind(&HttpRequest::finish, in)
			);
		}
		else
		{
			in->finish();
		}
	}

	return true;
}//}}}

std::pair<std::size_t, std::size_t> HttpCore::makeOffsets(const std::pair<std::size_t, std::size_t>& p, std::size_t actual_size)
{
	std::pair<std::size_t, std::size_t> q;

	if (p.first == HttpRangeDef::npos) // suffix-range-spec
	{
		q.first = actual_size - p.second;
		q.second = actual_size - 1;
	}
	else
	{
		q.first = p.first;

		q.second = p.second == HttpRangeDef::npos && p.second > actual_size
			? actual_size - 1
			: p.second;
	}

	return q;
}

/**
 * generates a boundary tag.
 *
 * \return a value usable as boundary tag.
 */
inline std::string HttpCore::generateBoundaryID() const
{
	static const char *map = "0123456789abcdef";
	char buf[16 + 1];

	for (std::size_t i = 0; i < sizeof(buf) - 1; ++i)
		buf[i] = map[random() % (sizeof(buf) - 1)];

	buf[sizeof(buf) - 1] = '\0';

	return std::string(buf);
}
// }}}

// {{{ post_config
bool HttpCore::post_config()
{
	if (emitLLVM_)
	{
		// XXX we could certainly dump this into a special destination instead to stderr.
		server().runner_->dump();
	}

	return true;
}
// }}}

// {{{ getrimit / setrlimit
static inline const char *rc2str(int resource)
{
	switch (resource)
	{
		case RLIMIT_CORE: return "core";
		case RLIMIT_AS: return "address-space";
		case RLIMIT_NOFILE: return "filedes";
		default: return "unknown";
	}
}

unsigned long long HttpCore::getrlimit(int resource)
{
	struct rlimit rlim;
	if (::getrlimit(resource, &rlim) == -1)
	{
		server().log(Severity::warn, "Failed to retrieve current resource limit on %s (%d).",
			rc2str(resource), resource);
		return 0;
	}
	return rlim.rlim_cur;
}

unsigned long long HttpCore::setrlimit(int resource, unsigned long long value)
{
	struct rlimit rlim;
	if (::getrlimit(resource, &rlim) == -1)
	{
		server().log(Severity::warn, "Failed to retrieve current resource limit on %s.", rc2str(resource), resource);

		return 0;
	}

	rlim_t last = rlim.rlim_cur;

	// patch against human readable form
	long long hlast = last, hvalue = value;

	if (value > RLIM_INFINITY)
		value = RLIM_INFINITY;

	rlim.rlim_cur = value;
	rlim.rlim_max = value;

	if (::setrlimit(resource, &rlim) == -1) {
		server().log(Severity::warn, "Failed to set resource limit on %s from %lld to %lld.", rc2str(resource), hlast, hvalue);

		return 0;
	}

	server().log(Severity::debug, "Set resource limit on %s from %lld to %lld.", rc2str(resource), hlast, hvalue);

	return value;
}
// }}}

// redirect physical request paths not ending with slash if mapped to directory
bool HttpCore::redirectOnIncompletePath(HttpRequest *in)
{
	std::string filename = in->fileinfo->filename();
	if (!in->fileinfo->isDirectory() || in->path.ends('/'))
		return false;

	std::stringstream url;

	BufferRef hostname(in->requestHeader("X-Forwarded-Host"));
	if (hostname.empty())
		hostname = in->requestHeader("Host");

	url << (in->connection.secure ? "https://" : "http://");
	url << hostname.str();
	url << in->path.str();
	url << '/';

	if (!in->query.empty())
		url << '?' << in->query.str();

	in->responseHeaders.overwrite("Location", url.str());
	in->status = HttpError::MovedPermanently;

	in->finish();
	return true;
}

} // namespace x0
