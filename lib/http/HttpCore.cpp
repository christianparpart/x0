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
#include <x0/SocketSpec.h>
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
#include <pwd.h>
#include <grp.h>

namespace x0 {

HttpCore::HttpCore(HttpServer& server) :
	HttpPlugin(server, "core"),
	emitLLVM_(false),
	max_fds(std::bind(&HttpCore::getrlimit, this, RLIMIT_NOFILE),
			std::bind(&HttpCore::setrlimit, this, RLIMIT_NOFILE, std::placeholders::_1))
{
	// setup
	registerSetupFunction<HttpCore, &HttpCore::emit_llvm>("llvm.dump", FlowValue::VOID);
	registerSetupFunction<HttpCore, &HttpCore::listen>("listen", FlowValue::BOOLEAN);
	registerSetupFunction<HttpCore, &HttpCore::log_sd>("log.systemd", FlowValue::VOID);
	registerSetupProperty<HttpCore, &HttpCore::loglevel>("log.level", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::logfile>("log.file", FlowValue::STRING);
	registerSetupFunction<HttpCore, &HttpCore::user>("user", FlowValue::BOOLEAN);
	registerSetupProperty<HttpCore, &HttpCore::workers>("workers", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::mimetypes>("mimetypes", FlowValue::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::mimetypes_default>("mimetypes.default", FlowValue::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::etag_mtime>("etag.mtime", FlowValue::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::etag_size>("etag.size", FlowValue::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::etag_inode>("etag.inode", FlowValue::VOID); // write-only (array)
	registerSetupProperty<HttpCore, &HttpCore::fileinfo_cache_ttl>("fileinfo.ttl", FlowValue::VOID); // write-only (int)
	registerSetupProperty<HttpCore, &HttpCore::server_advertise>("server.advertise", FlowValue::BOOLEAN);
	registerSetupProperty<HttpCore, &HttpCore::server_tags>("server.tags", FlowValue::VOID); // write-only (array)

	registerSetupProperty<HttpCore, &HttpCore::max_read_idle>("max_read_idle", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_write_idle>("max_write_idle", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_keepalive_idle>("max_keepalive_idle", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_keepalive_requests>("max_keepalive_requests", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_conns>("max_connections", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_files>("max_files", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_address_space>("max_address_space", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_core>("max_core_size", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::tcp_cork>("tcp_cork", FlowValue::BOOLEAN);
	registerSetupProperty<HttpCore, &HttpCore::tcp_nodelay>("tcp_nodelay", FlowValue::BOOLEAN);
	registerSetupProperty<HttpCore, &HttpCore::max_request_uri_size>("max_request_uri_size", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_request_header_size>("max_request_header_size", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_request_header_count>("max_request_header_count", FlowValue::NUMBER);
	registerSetupProperty<HttpCore, &HttpCore::max_request_body_size>("max_request_body_size", FlowValue::NUMBER);

	// TODO setup error-documents

	// shared
	registerSharedFunction<HttpCore, &HttpCore::sys_env>("sys.env", FlowValue::STRING);
	registerSharedProperty<HttpCore, &HttpCore::sys_cwd>("sys.cwd", FlowValue::STRING);
	registerSharedProperty<HttpCore, &HttpCore::sys_pid>("sys.pid", FlowValue::NUMBER);
	registerSharedProperty<HttpCore, &HttpCore::sys_now>("sys.now", FlowValue::NUMBER);
	registerSharedProperty<HttpCore, &HttpCore::sys_now_str>("sys.now_str", FlowValue::STRING);

	registerSharedFunction<HttpCore, &HttpCore::log_err>("log.err", FlowValue::VOID);
	registerSharedFunction<HttpCore, &HttpCore::log_info>("log.info", FlowValue::VOID);
	registerSharedFunction<HttpCore, &HttpCore::log_info>("log", FlowValue::VOID);
	registerSharedFunction<HttpCore, &HttpCore::log_debug>("log.debug", FlowValue::VOID);

	// main
	registerHandler<HttpCore, &HttpCore::docroot>("docroot");
	registerHandler<HttpCore, &HttpCore::alias>("alias");
	registerFunction<HttpCore, &HttpCore::autoindex>("autoindex", FlowValue::VOID);
	registerFunction<HttpCore, &HttpCore::rewrite>("rewrite", FlowValue::VOID);
	registerFunction<HttpCore, &HttpCore::pathinfo>("pathinfo", FlowValue::VOID);
	registerFunction<HttpCore, &HttpCore::error_handler>("error.handler", FlowValue::VOID);
	registerProperty<HttpCore, &HttpCore::req_method>("req.method", FlowValue::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_url>("req.url", FlowValue::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_path>("req.path", FlowValue::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_header>("req.header", FlowValue::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_host>("req.host", FlowValue::BUFFER);
	registerProperty<HttpCore, &HttpCore::req_pathinfo>("req.pathinfo", FlowValue::STRING);
	registerProperty<HttpCore, &HttpCore::req_is_secure>("req.is_secure", FlowValue::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::req_scheme>("req.scheme", FlowValue::STRING);
	registerProperty<HttpCore, &HttpCore::req_status_code>("req.status", FlowValue::NUMBER);
	registerProperty<HttpCore, &HttpCore::conn_remote_ip>("req.remoteip", FlowValue::STRING);
	registerProperty<HttpCore, &HttpCore::conn_remote_port>("req.remoteport", FlowValue::NUMBER);
	registerProperty<HttpCore, &HttpCore::conn_local_ip>("req.localip", FlowValue::STRING);
	registerProperty<HttpCore, &HttpCore::conn_local_port>("req.localport", FlowValue::NUMBER);
	registerProperty<HttpCore, &HttpCore::phys_path>("phys.path", FlowValue::STRING);
	registerProperty<HttpCore, &HttpCore::phys_exists>("phys.exists", FlowValue::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_is_reg>("phys.is_reg", FlowValue::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_is_dir>("phys.is_dir", FlowValue::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_is_exe>("phys.is_exe", FlowValue::BOOLEAN);
	registerProperty<HttpCore, &HttpCore::phys_mtime>("phys.mtime", FlowValue::NUMBER);
	registerProperty<HttpCore, &HttpCore::phys_size>("phys.size", FlowValue::NUMBER);
	registerProperty<HttpCore, &HttpCore::phys_etag>("phys.etag", FlowValue::STRING);
	registerProperty<HttpCore, &HttpCore::phys_mimetype>("phys.mimetype", FlowValue::STRING);

	registerFunction<HttpCore, &HttpCore::header_add>("header.add", FlowValue::VOID);
	registerFunction<HttpCore, &HttpCore::header_append>("header.append", FlowValue::VOID);
	registerFunction<HttpCore, &HttpCore::header_overwrite>("header.overwrite", FlowValue::VOID);
	registerFunction<HttpCore, &HttpCore::header_remove>("header.remove", FlowValue::VOID);

	// main handlers
	registerHandler<HttpCore, &HttpCore::staticfile>("staticfile");
	registerHandler<HttpCore, &HttpCore::redirect>("redirect");
	registerHandler<HttpCore, &HttpCore::respond>("respond");
	registerHandler<HttpCore, &HttpCore::blank>("blank");
}

HttpCore::~HttpCore()
{
}

// {{{ setup
void HttpCore::user(const FlowParams& args, FlowValue& result)
{
	std::string username(args.size() >= 1 ? args[0].toString() : "");
	std::string groupname(args.size() == 2 ? args[1].toString() : "");

	result.set(drop_privileges(username, groupname));
}

/** drops runtime privileges current process to given user's/group's name. */
bool HttpCore::drop_privileges(const std::string& username, const std::string& groupname)
{
	if (!groupname.empty() && !getgid()) {
		if (struct group *gr = getgrnam(groupname.c_str())) {
			if (setgid(gr->gr_gid) != 0) {
				log(Severity::error, "could not setgid to %s: %s", groupname.c_str(), strerror(errno));
				return false;
			}

			setgroups(0, nullptr);

			if (!username.empty()) {
				initgroups(username.c_str(), gr->gr_gid);
			}
		} else {
			log(Severity::error, "Could not find group: %s", groupname.c_str());
			return false;
		}
	}

	if (!username.empty() && !getuid()) {
		if (struct passwd *pw = getpwnam(username.c_str())) {
			if (setuid(pw->pw_uid) != 0) {
				log(Severity::error, "could not setgid to %s: %s", username.c_str(), strerror(errno));
				return false;
			}
			log(Severity::info, "Dropped privileges to user %s", username.c_str());

			if (chdir(pw->pw_dir) < 0) {
				log(Severity::error, "could not chdir to %s: %s", pw->pw_dir, strerror(errno));
				return false;
			}
		} else {
			log(Severity::error, "Could not find group: %s", groupname.c_str());
			return false;
		}
	}
	return true;
}

void HttpCore::mimetypes(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isString())
	{
		server().fileinfoConfig_.loadMimetypes(args[0].toString());
	}
}

void HttpCore::mimetypes_default(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isString())
	{
		server().fileinfoConfig_.defaultMimetype = args[0].toString();
	}
}

void HttpCore::etag_mtime(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().fileinfoConfig_.etagConsiderMtime = args[0].toBool();
	else
		result.set(server().fileinfoConfig_.etagConsiderMtime);
}

void HttpCore::etag_size(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().fileinfoConfig_.etagConsiderSize = args[0].toBool();
	else
		result.set(server().fileinfoConfig_.etagConsiderSize);
}

void HttpCore::etag_inode(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().fileinfoConfig_.etagConsiderInode = args[0].toBool();
	else
		result.set(server().fileinfoConfig_.etagConsiderInode);
}

void HttpCore::fileinfo_cache_ttl(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		server().fileinfoConfig_.cacheTTL_ = args[0].toNumber();
	else
		result.set(server().fileinfoConfig_.cacheTTL_);
}

void HttpCore::server_advertise(const FlowParams& args, FlowValue& result)
{
	if (args.empty())
	{
		result.set(server().advertise());
	}
	else // TODO if (args.expect(FlowValue::NUMBER))
	{
		server().advertise(args[0].toBool());
	}
}

void HttpCore::server_tags(const FlowParams& args, FlowValue& result)
{
	for (size_t i = 0, e = args.size(); i != e; ++i)
		loadServerTag(args[i]);
}

void HttpCore::loadServerTag(const FlowValue& tag)
{
	switch (tag.type())
	{
		case FlowValue::ARRAY:
			for (auto a: tag.toArray())
				loadServerTag(a);
			break;
		case FlowValue::STRING:
			if (*tag.toString() != '\0')
				server().components_.push_back(tag.toString());
			break;
		case FlowValue::BUFFER:
			if (tag.toNumber() > 0)
				server().components_.push_back(std::string(tag.toString(), tag.toNumber()));
			break;
		default:
			;//TODO reportWarning("Skip invalid value in server.tag: '%s'.", d->dump().c_str());
	}
}

void HttpCore::max_read_idle(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		server().maxReadIdle(TimeSpan::fromSeconds(args[0].toNumber()));
	else
		result.set(server().maxReadIdle());
}

void HttpCore::max_write_idle(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		server().maxWriteIdle(TimeSpan::fromSeconds(args[0].toNumber()));
	else
		result.set(server().maxWriteIdle());
}

void HttpCore::max_keepalive_idle(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		server().maxKeepAlive(TimeSpan::fromSeconds(args[0].toNumber()));
	else
		result.set(server().maxKeepAlive());
}

void HttpCore::max_keepalive_requests(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		server().maxKeepAliveRequests(args[0].toNumber());
	else
		result.set(server().maxKeepAliveRequests());
}

void HttpCore::max_conns(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		server().maxConnections(args[0].toNumber());
	else
		result.set(server().maxConnections());
}

void HttpCore::max_files(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		setrlimit(RLIMIT_NOFILE, args[0].toNumber());
	else
		result.set(getrlimit(RLIMIT_NOFILE));
}

void HttpCore::max_address_space(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		setrlimit(RLIMIT_AS, args[0].toNumber());
	else
		result.set(getrlimit(RLIMIT_AS));
}

void HttpCore::max_core(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isNumber())
		setrlimit(RLIMIT_CORE, args[0].toNumber());
	else
		result.set(getrlimit(RLIMIT_CORE));
}

void HttpCore::tcp_cork(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().tcpCork(args[0].toBool());
	else
		result.set(server().tcpCork());
}

void HttpCore::tcp_nodelay(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().tcpNoDelay(args[0].toBool());
	else
		result.set(server().tcpNoDelay());
}

void HttpCore::max_request_uri_size(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().maxRequestUriSize(args[0].toBool());
	else
		result.set(server().maxRequestUriSize());
}

void HttpCore::max_request_header_size(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().maxRequestHeaderSize(args[0].toBool());
	else
		result.set(server().maxRequestHeaderSize());
}

void HttpCore::max_request_header_count(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().maxRequestHeaderCount(args[0].toBool());
	else
		result.set(server().maxRequestHeaderCount());
}

void HttpCore::max_request_body_size(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1 && args[0].isBool())
		server().maxRequestBodySize(args[0].toBool());
	else
		result.set(server().maxRequestBodySize());
}

void HttpCore::listen(const FlowParams& args, FlowValue& result)
{
	SocketSpec socketSpec;
	socketSpec << args;

	if (!socketSpec.valid) {
		result.set(false);
	} else {
		result.set(server().setupListener(socketSpec));
	}
}

void HttpCore::workers(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1) {
		if (args[0].isArray()) {
			size_t i = 0;
			size_t count = server_.workers_.size();

			// spawn or set affinity of a set of workers as passed via input array
			for (auto value: args[0].toArray()) {
				if (value.isNumber()) {
					if (i >= count)
						server_.spawnWorker();

					server_.workers_[i]->setAffinity(value.toNumber());
					++i;
				}
			}

			// destroy workers that exceed our input array
			for (count = server_.workers_.size(); i < count; --count) {
				server_.destroyWorker(server_.workers_[count - 1]);
			}
		} else {
			size_t cur = server_.workers_.size();
			size_t count = args.size() == 1 ? args[0].toNumber() : 1;

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

void HttpCore::logfile(const FlowParams& args, FlowValue& result)
{
	if (args.size() == 1)
	{
		if (args[0].isString())
		{
			const char *filename = args[0].toString();
			auto nowfn = std::bind(&global_now);
			server_.logger_.reset(new FileLogger<decltype(nowfn)>(filename, nowfn));
		}
	}
}

void HttpCore::log_sd(const FlowParams& args, FlowValue& result)
{
	server_.logger_.reset(new SystemdLogger());
}

void HttpCore::loglevel(const FlowParams& args, FlowValue& result)
{
	if (args.empty()) {
		result.set(server().logLevel());
	} else if (args.size() == 1 && args[0].isNumber()) {
		int level = args[0].toNumber();
		server().logLevel(static_cast<Severity>(level));
	}
}

void HttpCore::emit_llvm(const FlowParams& args, FlowValue& result)
{
	emitLLVM_ = true;
}
// }}}

// {{{ sys
void HttpCore::sys_env(HttpRequest*, const FlowParams& args, FlowValue& result)
{
	result.set(getenv(args[0].toString()));
}

void HttpCore::sys_cwd(HttpRequest*, const FlowParams& args, FlowValue& result)
{
	static char buf[1024];
	result.set(getcwd(buf, sizeof(buf)));
}

void HttpCore::sys_pid(HttpRequest*, const FlowParams& args, FlowValue& result)
{
	result.set(getpid());
}

void HttpCore::sys_now(HttpRequest*, const FlowParams& args, FlowValue& result)
{
	result.set(static_cast<uint64_t>(server().workers_[0]->now_.unixtime()));
}

void HttpCore::sys_now_str(HttpRequest*, const FlowParams& args, FlowValue& result)
{
	auto& s = server().workers_[0]->now_.http_str();
	result.set(s.data(), s.size());
}
// }}}

// {{{ log.*
void HttpCore::log_err(HttpRequest* r, const FlowParams& args, FlowValue& /*result*/)
{
	if (r)
		r->log(Severity::error, "%s", args[0].toString());
	else
		server().log(Severity::error, "%s", args[0].toString());
}

void HttpCore::log_info(HttpRequest* r, const FlowParams& args, FlowValue& /*result*/)
{
	if (r)
		r->log(Severity::info, "%s", args[0].toString());
	else
		server().log(Severity::info, "%s", args[0].toString());
}

void HttpCore::log_debug(HttpRequest* r, const FlowParams& args, FlowValue& /*result*/)
{
	if (r)
		r->log(Severity::debug, "%s", args[0].toString());
	else
		server().log(Severity::debug, "%s", args[0].toString());
}
// }}}

// {{{ req
void HttpCore::autoindex(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	if (in->documentRoot.empty()) {
		server().log(Severity::error, "autoindex: No document root set yet. Skipping.");
		return; // error: must have a document-root set first.
	}

	if (!in->fileinfo)
		return; // something went wrong, just be sure we SEGFAULT here

	if (!in->fileinfo->isDirectory())
		return;

	if (args.size() < 1)
		return;

	for (size_t i = 0, e = args.size(); i != e; ++i)
		if (matchIndex(in, args[i]))
			return;
}

bool HttpCore::matchIndex(HttpRequest *in, const FlowValue& arg)
{
	std::string path(in->fileinfo->path());

	switch (arg.type())
	{
		case FlowValue::STRING:
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
		case FlowValue::ARRAY:
		{
			for (auto a: arg.toArray())
				if (matchIndex(in, a))
					return true;

			break;
		}
		default:
			break;
	}
	return false;
}

bool HttpCore::docroot(HttpRequest* in, const FlowParams& args)
{
	if (args.size() != 1)
		return false;

	in->documentRoot = args[0].toString();
	in->fileinfo = in->connection.worker().fileinfo(in->documentRoot + in->path.str());
	// XXX; we could autoindex here in case the user told us an autoindex before the docroot.

	return redirectOnIncompletePath(in);
}

bool HttpCore::alias(HttpRequest* in, const FlowParams& args)
{
	if (args.size() != 1 || !args[0].isArray()) {
		server().log(Severity::error, "alias: invalid argument count");
		return false;
	}

	const FlowArray& r = args[0].toArray();

	if (r.size() != 2)
		return false;

	if (!r[0].isString() || !r[1].isString()) {
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

	std::string prefix = r[0].toString();
	size_t prefixLength = prefix.size();
	std::string alias = r[1].toString();

	if (in->path.begins(prefix))
		in->fileinfo = in->connection.worker().fileinfo(alias + in->path.substr(prefixLength));

	return redirectOnIncompletePath(in);
}

void HttpCore::rewrite(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	if (!args.size())
		return;

	in->fileinfo = in->connection.worker().fileinfo(in->documentRoot + args[0].toString());
}

void HttpCore::pathinfo(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	if (!in->fileinfo) {
		in->log(Severity::error,
				"pathinfo: no file information available. Please set document root first.");
		return;
	}

	in->updatePathInfo();
}

void HttpCore::error_handler(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	in->setErrorHandler(args[0].toFunction());
}

void HttpCore::req_method(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->method.data(), in->method.size());
}

void HttpCore::req_url(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->uri.data(), in->uri.size());
}

void HttpCore::req_path(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->path.data(), in->path.size());
}

void HttpCore::req_header(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	BufferRef ref(in->requestHeader(args[0].toString()));
	result.set(ref.data(), ref.size());
}

void HttpCore::req_host(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->hostname.data(), in->hostname.size());
}

void HttpCore::req_pathinfo(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->pathinfo.c_str();
}

void HttpCore::req_is_secure(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->connection.isSecure();
}

void HttpCore::req_scheme(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->connection.isSecure() ? "https" : "http";
}

void HttpCore::req_status_code(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = static_cast<unsigned>(in->status);
}
// }}}

// {{{ connection
void HttpCore::conn_remote_ip(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->connection.remoteIP().c_str();
}

void HttpCore::conn_remote_port(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->connection.remotePort();
}

void HttpCore::conn_local_ip(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->connection.localIP().c_str();
}

void HttpCore::conn_local_port(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result = in->connection.localPort();
}
// }}}

// {{{ phys
void HttpCore::phys_path(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->path().c_str() : "");
}

void HttpCore::phys_exists(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->exists() : false);
}

void HttpCore::phys_is_reg(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->isRegular() : false);
}

void HttpCore::phys_is_dir(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->isDirectory() : false);
}

void HttpCore::phys_is_exe(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->isExecutable() : false);
}

void HttpCore::phys_mtime(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(static_cast<uint64_t>(in->fileinfo ? in->fileinfo->mtime() : 0));
}

void HttpCore::phys_size(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->size() : 0);
}

void HttpCore::phys_etag(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->etag().c_str() : "");
}

void HttpCore::phys_mimetype(HttpRequest* in, const FlowParams& args, FlowValue& result)
{
	result.set(in->fileinfo ? in->fileinfo->mimetype().c_str() : "");
}
// }}}

// {{{ handler
bool HttpCore::redirect(HttpRequest *in, const FlowParams& args)
{
	in->status = HttpError::MovedTemporarily;
	in->responseHeaders.overwrite("Location", args[0].toString());
	in->finish();

	return true;
}

bool HttpCore::respond(HttpRequest *in, const FlowParams& args)
{
	if (args.size() >= 1 && args[0].isNumber())
		in->status = static_cast<HttpError>(args[0].toNumber());

	in->finish();
	return true;
}

bool HttpCore::blank(HttpRequest* in, const FlowParams& args)
{
	in->status = HttpError::Ok;
	in->finish();
	return true;
}
// }}}

// {{{ response's header.* functions
void HttpCore::header_add(HttpRequest* r, const FlowParams& args, FlowValue& result)
{
	if (args.size() != 2)
		return;

	if (args[0].isString() && args[1].isString()) {
		r->responseHeaders.push_back(args[0].toString(), args[1].toString());
	}
}

// header.append(headerName, appendValue)
void HttpCore::header_append(HttpRequest* r, const FlowParams& args, FlowValue& result)
{
	if (args.size() != 2)
		return;

	if (args[0].isString() && args[1].isString()) {
		r->responseHeaders[args[0].toString()] += args[1].toString();
	}
}

void HttpCore::header_overwrite(HttpRequest* r, const FlowParams& args, FlowValue& result)
{
	if (args.size() != 2)
		return;

	if (args[0].isString() && args[1].isString()) {
		r->responseHeaders.overwrite(args[0].toString(), args[1].toString());
	}
}

void HttpCore::header_remove(HttpRequest* r, const FlowParams& args, FlowValue& result)
{
	if (args.size() != 1)
		return;

	if (args[0].isString()) {
		r->responseHeaders.remove(args[0].toString());
	}
}
// }}}

// {{{ staticfile handler
bool HttpCore::staticfile(HttpRequest *in, const FlowParams& args) // {{{
{
	if (!in->fileinfo)
		return false;

	if (!in->fileinfo->exists())
		return false;

	if (!in->fileinfo->isRegular())
		return false;

	if (in->status != HttpError::Undefined) {
		// processing an internal redirect (to a static file)

		in->responseHeaders.push_back("Last-Modified", in->fileinfo->lastModified());
		in->responseHeaders.push_back("ETag", in->fileinfo->etag());

		in->responseHeaders.push_back("Content-Type", in->fileinfo->mimetype());
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(in->fileinfo->size()));

		int fd = in->fileinfo->open(O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			log(Severity::error, "Could not open file: '%s': %s", in->fileinfo->filename().c_str(), strerror(errno));
			return false;
		}

		posix_fadvise(fd, 0, in->fileinfo->size(), POSIX_FADV_SEQUENTIAL);
		in->write<FileSource>(fd, 0, in->fileinfo->size(), true);
		in->finish();

		return true;
	}

	in->status = verifyClientCache(in);
	if (in->status != HttpError::Ok) {
		in->finish();
		return true;
	}

	int fd = -1;
	if (equals(in->method, "GET")) {
		int flags = O_RDONLY | O_NONBLOCK;

#if defined(O_CLOEXEC)
		flags |= O_CLOEXEC;
#endif

		fd = in->fileinfo->open(flags);

		if (fd < 0) {
			server_.log(Severity::error, "Could not open file '%s': %s",
				in->fileinfo->path().c_str(), strerror(errno));

			in->status = HttpError::Forbidden;
			in->finish();

			return true;
		}
	} else if (!equals(in->method, "HEAD")) {
		in->status = HttpError::MethodNotAllowed;
		in->finish();

		return true;
	}

	in->responseHeaders.push_back("Last-Modified", in->fileinfo->lastModified());
	in->responseHeaders.push_back("ETag", in->fileinfo->etag());

	if (!processRangeRequest(in, fd)) {
		in->responseHeaders.push_back("Accept-Ranges", "bytes");
		in->responseHeaders.push_back("Content-Type", in->fileinfo->mimetype());
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(in->fileinfo->size()));

		if (fd < 0) { // HEAD request
			in->finish();
		} else {
			posix_fadvise(fd, 0, in->fileinfo->size(), POSIX_FADV_SEQUENTIAL);

			in->write<FileSource>(fd, 0, in->fileinfo->size(), true);
			in->finish();
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

/*! fully processes the ranged requests, if one, or does nothing.
 *
 * \retval true this was a ranged request and we fully processed it (invoked finish())
 * \internal false this is no ranged request. nothing is done on it.
 */
inline bool HttpCore::processRangeRequest(HttpRequest *in, int fd) //{{{
{
	BufferRef range_value(in->requestHeader("Range"));
	HttpRangeDef range;

	// if no range request or range request was invalid (by syntax) we fall back to a full response
	if (range_value.empty() || !range.parse(range_value))
		return false;

	BufferRef ifRangeCond(in->requestHeader("If-Range"));
	if (!ifRangeCond.empty()) {
		//printf("If-Range specified: %s\n", ifRangeCond.str().c_str());
		if (!equals(ifRangeCond, in->fileinfo->etag())
				&& !equals(ifRangeCond, in->fileinfo->lastModified())) {
			//printf("-> does not equal\n");
			return false;
		}
	}

	in->status = HttpError::PartialContent;

	if (range.size() > 1) {
		// generate a multipart/byteranged response, as we've more than one range to serve

		CompositeSource* content = new CompositeSource();
		Buffer buf;
		std::string boundary(generateBoundaryID());
		std::size_t contentLength = 0;

		for (int i = 0, e = range.size(); i != e; ++i) {
			std::pair<std::size_t, std::size_t> offsets(makeOffsets(range[i], in->fileinfo->size()));
			if (offsets.second < offsets.first) {
				in->status = HttpError::RequestedRangeNotSatisfiable;
				in->finish();
				return true;
			}

			std::size_t partLength = 1 + offsets.second - offsets.first;

			buf.clear();
			buf.push_back("\r\n--");
			buf.push_back(boundary);
			buf.push_back("\r\nContent-Type: ");
			buf.push_back(in->fileinfo->mimetype());

			buf.push_back("\r\nContent-Range: bytes ");
			buf.push_back(offsets.first);
			buf.push_back("-");
			buf.push_back(offsets.second);
			buf.push_back("/");
			buf.push_back(in->fileinfo->size());
			buf.push_back("\r\n\r\n");

			contentLength += buf.size() + partLength;

			if (fd >= 0) {
				bool lastChunk = i + 1 == e;
				content->push_back<BufferSource>(std::move(buf));
				content->push_back<FileSource>(fd, offsets.first, partLength, lastChunk);
			}
		}

		buf.clear();
		buf.push_back("\r\n--");
		buf.push_back(boundary);
		buf.push_back("--\r\n");

		contentLength += buf.size();
		content->push_back<BufferSource>(std::move(buf));

		// push the prepared ranged response into the client
		char slen[32];
		snprintf(slen, sizeof(slen), "%ld", contentLength);

		in->responseHeaders.push_back("Content-Type", "multipart/byteranges; boundary=" + boundary);
		in->responseHeaders.push_back("Content-Length", slen);

		if (fd >= 0) {
			in->write(content);
		}
	}
	else // generate a simple (single) partial response
	{
		std::pair<std::size_t, std::size_t> offsets(makeOffsets(range[0], in->fileinfo->size()));
		if (offsets.second < offsets.first) {
			in->status = HttpError::RequestedRangeNotSatisfiable;
			in->finish();
			return true;
		}

		in->responseHeaders.push_back("Content-Type", in->fileinfo->mimetype());

		std::size_t length = 1 + offsets.second - offsets.first;

		char slen[32];
		snprintf(slen, sizeof(slen), "%ld", length);
		in->responseHeaders.push_back("Content-Length", slen);

		FixedBuffer<128> cr;
		cr << "bytes " << offsets.first << '-' << offsets.second << '/' << in->fileinfo->size();
		in->responseHeaders.push_back("Content-Range", cr.c_str());

		if (fd >= 0) {
			in->write<FileSource>(fd, offsets.first, length, true);
		}
	}

	in->finish();
	return true;
}//}}}

/*! converts a range-spec into real offsets.
 */
std::pair<std::size_t, std::size_t> HttpCore::makeOffsets(const std::pair<std::size_t, std::size_t>& p, std::size_t actualSize)
{
	std::pair<std::size_t, std::size_t> q;

	if (p.first == HttpRangeDef::npos) // suffix-range-spec
	{
		q.first = actualSize - p.second;
		q.second = actualSize - 1;
	}
	else
	{
		q.first = p.first;

		q.second = p.second == HttpRangeDef::npos && p.second > actualSize
			? actualSize - 1
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
	if (!in->fileinfo->isDirectory() || in->path.ends('/'))
		return false;

	std::stringstream url;

	BufferRef hostname(in->requestHeader("X-Forwarded-Host"));
	if (hostname.empty())
		hostname = in->requestHeader("Host");

	url << (in->connection.isSecure() ? "https://" : "http://");
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
