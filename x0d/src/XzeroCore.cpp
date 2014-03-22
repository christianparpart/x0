/* <x0/XzeroCore.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0d/XzeroCore.h>
#include <x0d/XzeroDaemon.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpRangeDef.h>
#include <x0/io/CompositeSource.h>
#include <x0/io/BufferSource.h>
#include <x0/io/FileSource.h>
#include <x0/Tokenizer.h>
#include <x0/SocketSpec.h>
#include <x0/Types.h>
#include <x0/DateTime.h>
#include <x0/Logger.h>
#include <x0/strutils.h>

#include <sstream>
#include <cstdio>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#include "sd-daemon.h"

namespace x0d {

using namespace x0;

typedef x0::FlowVM::Params FlowParams;

static Buffer concat(FlowParams& args)
{
	Buffer msg;

	for (size_t i = 1, e = args.size(); i != e; ++i) {
		if (i > 1)
			msg << " ";

        msg.push_back(*(FlowString*) args[i]);
	}

	return std::move(msg);
}

XzeroCore::XzeroCore(XzeroDaemon* d) :
	XzeroPlugin(d, "core"),
	emitIR_(false),
	max_fds(std::bind(&XzeroCore::getrlimit, this, RLIMIT_NOFILE),
			std::bind(&XzeroCore::setrlimit, this, RLIMIT_NOFILE, std::placeholders::_1))
{
	// setup: functions
    setupFunction("ir.dump", &XzeroCore::dump_ir);
    setupFunction("listen", &XzeroCore::listen)
        .param<IPAddress>("address", IPAddress("0.0.0.0"))
        .param<int>("port", 80)
        .param<int>("backlog", 128)
        .param<int>("multi_accept", 1)
        .param<bool>("reuse_port", false);

    // setup: properties (write-only)
    setupFunction("workers", &XzeroCore::workers, FlowType::Number);
    setupFunction("workers.affinity", &XzeroCore::workers_affinity, FlowType::IntArray);
    setupFunction("mimetypes", &XzeroCore::mimetypes, FlowType::String);
    setupFunction("mimetypes.default", &XzeroCore::mimetypes_default, FlowType::String);
    setupFunction("etag.mtime", &XzeroCore::etag_mtime, FlowType::Boolean);
    setupFunction("etag.size", &XzeroCore::etag_size, FlowType::Boolean);
    setupFunction("etag.inode", &XzeroCore::etag_inode, FlowType::Boolean);
    setupFunction("fileinfo.ttl", &XzeroCore::fileinfo_cache_ttl, FlowType::Number);
    setupFunction("server.advertise", &XzeroCore::server_advertise, FlowType::Boolean);
    setupFunction("server.tags", &XzeroCore::server_tags, FlowType::StringArray, FlowType::String);
    setupFunction("max_read_idle", &XzeroCore::max_read_idle, FlowType::Number);
    setupFunction("max_write_idle", &XzeroCore::max_write_idle, FlowType::Number);
    setupFunction("max_keepalive_idle", &XzeroCore::max_keepalive_idle, FlowType::Number);
    setupFunction("max_keepalive_requests", &XzeroCore::max_keepalive_requests, FlowType::Number);
    setupFunction("max_connections", &XzeroCore::max_conns, FlowType::Number);
    setupFunction("max_files", &XzeroCore::max_files, FlowType::Number);
    setupFunction("max_address_space", &XzeroCore::max_address_space, FlowType::Number);
    setupFunction("max_core_size", &XzeroCore::max_core, FlowType::Number);
    setupFunction("tcp_cork", &XzeroCore::tcp_cork, FlowType::Boolean);
    setupFunction("tcp_nodelay", &XzeroCore::tcp_nodelay, FlowType::Boolean);
    setupFunction("lingering", &XzeroCore::lingering, FlowType::Number);
    setupFunction("max_request_uri_size", &XzeroCore::max_request_uri_size, FlowType::Number);
    setupFunction("max_request_header_size", &XzeroCore::max_request_header_size, FlowType::Number);
    setupFunction("max_request_header_count", &XzeroCore::max_request_header_count, FlowType::Number);
    setupFunction("max_request_body_size", &XzeroCore::max_request_body_size, FlowType::Number);
    setupFunction("request_header_buffer_size", &XzeroCore::request_header_buffer_size, FlowType::Number);
    setupFunction("request_body_buffer_size", &XzeroCore::request_body_buffer_size, FlowType::Number);

	// TODO setup error-documents

	// shared properties (read-only)
    sharedFunction("systemd.booted", &XzeroCore::systemd_booted).returnType(FlowType::Boolean);
    sharedFunction("systemd.controlled", &XzeroCore::systemd_controlled).returnType(FlowType::Boolean);

    sharedFunction("sys.cpu_count", &XzeroCore::sys_cpu_count).returnType(FlowType::Number);
    sharedFunction("sys.env", &XzeroCore::sys_env).returnType(FlowType::String);
    sharedFunction("sys.cwd", &XzeroCore::sys_cwd).returnType(FlowType::String);
    sharedFunction("sys.pid", &XzeroCore::sys_pid).returnType(FlowType::String);
    sharedFunction("sys.now", &XzeroCore::sys_now).returnType(FlowType::String);
    sharedFunction("sys.now_str", &XzeroCore::sys_now_str).returnType(FlowType::String);

    sharedFunction("file.exists", &XzeroCore::file_exists).returnType(FlowType::Boolean);
    sharedFunction("file.is_reg", &XzeroCore::file_is_reg).returnType(FlowType::Boolean);
    sharedFunction("file.is_dir", &XzeroCore::file_is_dir).returnType(FlowType::Boolean);
    sharedFunction("file.is_exe", &XzeroCore::file_is_exe).returnType(FlowType::Boolean);

    sharedFunction("log.err", &XzeroCore::log_err, FlowType::String);
    sharedFunction("log.warn", &XzeroCore::log_warn, FlowType::String);
    sharedFunction("log.notice", &XzeroCore::log_notice, FlowType::String);
    sharedFunction("log.info", &XzeroCore::log_info, FlowType::String);
    sharedFunction("log", &XzeroCore::log_info, FlowType::String);
    sharedFunction("log.debug", &XzeroCore::log_debug, FlowType::String);

	// main: read-only attributes
	mainFunction("req.method", &XzeroCore::req_method).returnType(FlowType::String);
	mainFunction("req.url", &XzeroCore::req_url).returnType(FlowType::String);
	mainFunction("req.path", &XzeroCore::req_path).returnType(FlowType::String);
	mainFunction("req.query", &XzeroCore::req_query).returnType(FlowType::String);
	mainFunction("req.header", &XzeroCore::req_header).returnType(FlowType::String);
	mainFunction("req.cookie", &XzeroCore::req_cookie).returnType(FlowType::String);
	mainFunction("req.host", &XzeroCore::req_host).returnType(FlowType::String);
	mainFunction("req.pathinfo", &XzeroCore::req_pathinfo).returnType(FlowType::String);
	mainFunction("req.is_secure", &XzeroCore::req_is_secure).returnType(FlowType::Boolean);
	mainFunction("req.scheme", &XzeroCore::req_scheme).returnType(FlowType::String);
	mainFunction("req.status", &XzeroCore::req_status_code).returnType(FlowType::Number);
	mainFunction("req.remoteip", &XzeroCore::conn_remote_ip).returnType(FlowType::IPAddress);
	mainFunction("req.remoteport", &XzeroCore::conn_remote_port).returnType(FlowType::Number);
	mainFunction("req.localip", &XzeroCore::conn_local_ip).returnType(FlowType::IPAddress);
	mainFunction("req.localport", &XzeroCore::conn_local_port).returnType(FlowType::Number);
	mainFunction("phys.path", &XzeroCore::phys_path).returnType(FlowType::String);
	mainFunction("phys.exists", &XzeroCore::phys_exists).returnType(FlowType::Boolean);
	mainFunction("phys.is_reg", &XzeroCore::phys_is_reg).returnType(FlowType::Boolean);
	mainFunction("phys.is_dir", &XzeroCore::phys_is_dir).returnType(FlowType::Boolean);
	mainFunction("phys.is_exe", &XzeroCore::phys_is_exe).returnType(FlowType::Boolean);
	mainFunction("phys.mtime", &XzeroCore::phys_mtime).returnType(FlowType::Number);
	mainFunction("phys.size", &XzeroCore::phys_size).returnType(FlowType::Number);
	mainFunction("phys.etag", &XzeroCore::phys_etag).returnType(FlowType::String);
	mainFunction("phys.mimetype", &XzeroCore::phys_mimetype).returnType(FlowType::String);

    // main: getter functions
	mainFunction("regex.group", &XzeroCore::regex_group, FlowType::Number).returnType(FlowType::String);

    // main: manipulation functions
	mainFunction("header.add", &XzeroCore::header_add, FlowType::String, FlowType::String);
	mainFunction("header.append", &XzeroCore::header_append, FlowType::String, FlowType::String);
	mainFunction("header.overwrite", &XzeroCore::header_overwrite, FlowType::String, FlowType::String);
	mainFunction("header.remove", &XzeroCore::header_remove, FlowType::String);

	mainFunction("autoindex", &XzeroCore::autoindex, FlowType::StringArray);
	mainFunction("rewrite", &XzeroCore::rewrite, FlowType::String).returnType(FlowType::Boolean);
	mainFunction("pathinfo", &XzeroCore::pathinfo);
	mainFunction("error.handler", &XzeroCore::error_handler, FlowType::Handler);

	// main: handlers
	mainHandler("docroot", &XzeroCore::docroot, FlowType::String);
	mainHandler("alias", &XzeroCore::alias, FlowType::String, FlowType::String);
	mainHandler("staticfile", &XzeroCore::staticfile);
	mainHandler("precompressed", &XzeroCore::precompressed);
	mainHandler("redirect", &XzeroCore::redirect);
	mainHandler("respond", &XzeroCore::respond, FlowType::Number);
	mainHandler("echo", &XzeroCore::echo, FlowType::String);
	mainHandler("blank", &XzeroCore::blank);
}

XzeroCore::~XzeroCore()
{
}

// {{{ setup
void XzeroCore::mimetypes(FlowParams& args)
{
    server().fileinfoConfig_.openMimeTypes(args.get<FlowString>(1).str());
}

void XzeroCore::mimetypes_default(FlowParams& args)
{
    server().fileinfoConfig_.defaultMimetype = args.get<FlowString>(1).str();
}

void XzeroCore::etag_mtime(FlowParams& args)
{
    server().fileinfoConfig_.etagConsiderMtime = args.get<bool>(1);
}

void XzeroCore::etag_size(FlowParams& args)
{
    server().fileinfoConfig_.etagConsiderSize = args.get<bool>(1);
}

void XzeroCore::etag_inode(FlowParams& args)
{
    server().fileinfoConfig_.etagConsiderInode = args.get<bool>(1);
}

void XzeroCore::fileinfo_cache_ttl(FlowParams& args)
{
    server().fileinfoConfig_.cacheTTL = args.get<FlowNumber>(1);
}

void XzeroCore::server_advertise(FlowParams& args)
{
    server().advertise(args.get<bool>(1));
}

void XzeroCore::server_tags(FlowParams& args)
{
    FlowStringArray array = args.get<FlowStringArray>(1);
    for (const auto& arg: array) {
        daemon().components_.push_back(arg.str());
    }
}

void XzeroCore::max_read_idle(FlowParams& args)
{
    server().maxReadIdle(TimeSpan::fromSeconds(args.get<FlowNumber>(1)));
}

void XzeroCore::max_write_idle(FlowParams& args)
{
    server().maxWriteIdle(TimeSpan::fromSeconds(args.get<FlowNumber>(1)));
}

void XzeroCore::max_keepalive_idle(FlowParams& args)
{
    server().maxKeepAlive(TimeSpan::fromSeconds(args.get<FlowNumber>(1)));
}

void XzeroCore::max_keepalive_requests(FlowParams& args)
{
    server().maxKeepAliveRequests(args.get<FlowNumber>(1));
}

void XzeroCore::max_conns(FlowParams& args)
{
    server().maxConnections(args.get<FlowNumber>(1));
}

void XzeroCore::max_files(FlowParams& args)
{
    setrlimit(RLIMIT_NOFILE, args.get<FlowNumber>(1));
}

void XzeroCore::max_address_space(FlowParams& args)
{
    setrlimit(RLIMIT_AS, args.get<FlowNumber>(1));
}

void XzeroCore::max_core(FlowParams& args)
{
    setrlimit(RLIMIT_CORE, args.get<FlowNumber>(1));
}

void XzeroCore::tcp_cork(FlowParams& args)
{
    server().tcpCork(args.get<bool>(1));
}

void XzeroCore::tcp_nodelay(FlowParams& args)
{
    server().tcpNoDelay(args.get<bool>(1));
}

void XzeroCore::lingering(FlowParams& args)
{
    server().lingering = TimeSpan::fromSeconds(args.get<FlowNumber>(1));
}

void XzeroCore::max_request_uri_size(FlowParams& args)
{
    server().maxRequestUriSize(args.get<FlowNumber>(1));
}

void XzeroCore::max_request_header_size(FlowParams& args)
{
    server().maxRequestHeaderSize(args.get<FlowNumber>(1));
}

void XzeroCore::max_request_header_count(FlowParams& args)
{
    server().maxRequestHeaderCount(args.get<FlowNumber>(1));
}

void XzeroCore::max_request_body_size(FlowParams& args)
{
    server().maxRequestBodySize(args.get<FlowNumber>(1));
}

void XzeroCore::request_header_buffer_size(FlowParams& args)
{
    server().requestHeaderBufferSize(args.get<FlowNumber>(1));
}

void XzeroCore::request_body_buffer_size(FlowParams& args)
{
    server().requestBodyBufferSize(args.get<FlowNumber>(1));
}

void XzeroCore::listen(FlowParams& args)
{
    SocketSpec socketSpec(
        args.get<IPAddress>(1),  // bind addr
        args.get<FlowNumber>(2), // port
        args.get<FlowNumber>(3), // backlog
        args.get<FlowNumber>(4), // multi accept
        args.get<bool>(5)        // reuse port
    );

    server().setupListener(socketSpec);
}

void XzeroCore::workers(FlowParams& args)
{
    size_t cur = server_->workers().size();
    size_t count = args.get<FlowNumber>(1);

    if (count < 1)
        count = 1;

    while (cur < count) {
        server_->spawnWorker();
        ++cur;
    }

    while (cur > count) {
        --cur;
        server_->destroyWorker(server_->workers()[cur]);
    }
}

// "workers.affinity([I]V"
void XzeroCore::workers_affinity(FlowParams& args)
{
    const auto& affinities = args.get<FlowIntArray>(1);
    size_t cur = server_->workers().size();
    size_t count = affinities.size();

    if (count > 0) {
        // spawn or set affinity of a set of workers as passed via input array
        for (size_t i = 1, e = affinities.size(); i < e; ++i) {
            if (i >= cur)
                server_->spawnWorker();

            server_->workers()[i]->setAffinity(affinities[i]);
        }
    }

    // destroy workers that exceed our input array
    cur = server_->workers().size();
    while (cur > count) {
        --cur;
        server_->destroyWorker(server_->workers()[cur]);
    }
}


void XzeroCore::dump_ir(FlowParams& args)
{
	emitIR_ = true;
}
// }}}
// {{{ systemd.*
void XzeroCore::systemd_booted(HttpRequest*, FlowParams& args)
{
	args.setResult(sd_booted() == 0);
}

void XzeroCore::systemd_controlled(HttpRequest*, FlowParams& args)
{
	args.setResult(sd_booted() == 0 && getppid() == 1);
}
// }}}
// {{{ sys
void XzeroCore::sys_cpu_count(HttpRequest*, x0::FlowVM::Params& args)
{
    long numCPU = sysconf(_SC_NPROCESSORS_ONLN);

    if (numCPU < 0)
        numCPU = 1;

    args.setResult(numCPU);
}

void XzeroCore::sys_env(HttpRequest*, FlowParams& args)
{
    args.setResult(getenv(args.get<FlowString*>(1)->str().c_str()));
}


void XzeroCore::sys_cwd(HttpRequest*, FlowParams& args)
{
	static char buf[1024];
	args.setResult(getcwd(buf, sizeof(buf)));
}

void XzeroCore::sys_pid(HttpRequest*, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(getpid()));
}

void XzeroCore::sys_now(HttpRequest*, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(server().workers()[0]->now().unixtime()));
}

void XzeroCore::sys_now_str(HttpRequest* r, FlowParams& args)
{
	const auto& s = r->connection.worker().now().http_str();
	args.setResult(s.c_str());
}
// }}}
// {{{ log.*
void XzeroCore::log_err(HttpRequest* r, FlowParams& args)
{
	if (r)
		r->log(Severity::error, "%s", concat(args).c_str());
	else
		server().log(Severity::error, "%s", concat(args).c_str());
}

void XzeroCore::log_warn(HttpRequest* r, FlowParams& args)
{
	if (r)
		r->log(Severity::info, "%s", concat(args).c_str());
	else
		server().log(Severity::warning, "%s", concat(args).c_str());
}

void XzeroCore::log_notice(HttpRequest* r, FlowParams& args)
{
	if (r)
		r->log(Severity::info, "%s", concat(args).c_str());
	else
		server().log(Severity::notice, "%s", concat(args).c_str());
}

void XzeroCore::log_info(HttpRequest* r, FlowParams& args)
{
	if (r)
		r->log(Severity::info, "%s", concat(args).c_str());
	else
		server().log(Severity::info, "%s", concat(args).c_str());
}

void XzeroCore::log_debug(HttpRequest* r, FlowParams& args)
{
	if (r)
		r->log(Severity::debug, "%s", concat(args).c_str());
	else
		server().log(Severity::debug, "%s", concat(args).c_str());
}
// }}}
// {{{ req
void XzeroCore::autoindex(HttpRequest* r, FlowParams& args)
{
	if (r->documentRoot.empty()) {
		server().log(Severity::error, "autoindex: No document root set yet. Skipping.");
		return; // error: must have a document-root set first.
	}

	if (!r->fileinfo)
		return; // something went wrong, just be sure we SEGFAULT here

	if (!r->fileinfo->isDirectory())
		return;

    FlowStringArray indexfiles = args.get<FlowStringArray>(1);
	for (size_t i = 0, e = indexfiles.size(); i != e; ++i)
		if (matchIndex(r, indexfiles[i]))
			return;
}

bool XzeroCore::matchIndex(HttpRequest *r, const BufferRef& arg)
{
	std::string path(r->fileinfo->path());

    Buffer ipath;
    ipath.reserve(path.length() + 1 + arg.size());
    ipath.push_back(path);
    if (path[path.size() - 1] != '/')
        ipath.push_back("/");
    ipath.push_back(arg);

    if (auto fi = r->connection.worker().fileinfo(ipath)) {
        if (fi->isRegular()) {
            r->fileinfo = fi;
            return true;
        }
    }

	return false;
}

bool XzeroCore::docroot(HttpRequest* in, FlowParams& args)
{
	in->documentRoot = args.get<FlowString>(1).str();

	if (in->documentRoot.empty()) {
		in->log(Severity::error, "Setting empty document root is not allowed.");
		in->status = HttpStatus::InternalServerError;
		in->finish();
		return true;
	}

	// cut off trailing slash
	size_t trailerOffset = in->documentRoot.size() - 1;
	if (in->documentRoot[trailerOffset] == '/')
		in->documentRoot.resize(trailerOffset);

	in->fileinfo = in->connection.worker().fileinfo(in->documentRoot + in->path.str());
	// XXX; we could autoindex here in case the user told us an autoindex before the docroot.

	return redirectOnIncompletePath(in);
}

bool XzeroCore::alias(HttpRequest* in, FlowParams& args)
{
	// input:
	//    URI: /some/uri/path
	//    Alias '/some' => '/srv/special';
	//
	// output:
	//    docroot: /srv/special
	//    fileinfo: /srv/special/uri/path

	std::string prefix = args.get<FlowString>(1).str();
	size_t prefixLength = prefix.size();
	std::string alias = args.get<FlowString>(2).str();

	if (in->path.begins(prefix))
		in->fileinfo = in->connection.worker().fileinfo(alias + in->path.substr(prefixLength));

	return redirectOnIncompletePath(in);
}

void XzeroCore::rewrite(HttpRequest* in, FlowParams& args)
{
    in->fileinfo = in->connection.worker().fileinfo(in->documentRoot + args.get<FlowString*>(1)->str());

    args.setResult(in->fileinfo ? in->fileinfo->exists() : false);
}

void XzeroCore::pathinfo(HttpRequest* r, FlowParams& args)
{
    if (!r->fileinfo) {
        r->log(Severity::error, "pathinfo: no file information available. Please set document root first.");
        return;
    }

    r->updatePathInfo();
}

void XzeroCore::error_handler(HttpRequest* r, FlowParams& args)
{
    FlowVM::Handler* handler = args.get<FlowVM::Handler*>(1);

    r->setErrorHandler([=](HttpRequest* r) -> bool { return handler->run(r); });
}


void XzeroCore::req_method(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->method);
}

void XzeroCore::req_url(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->unparsedUri);
}

void XzeroCore::req_path(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->path);
}

void XzeroCore::req_query(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->query);
}

void XzeroCore::req_header(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->requestHeader(args.get<FlowString>(1)));
}

void XzeroCore::req_cookie(HttpRequest* in, FlowParams& args)
{
	BufferRef cookie(in->requestHeader("Cookie"));
	if (!cookie.empty()) {
		auto wanted = args.get<FlowString>(1);
		static const std::string sld("; \t");
		Tokenizer<BufferRef> st1(cookie, sld);
		BufferRef kv;

		while (!(kv = st1.nextToken()).empty()) {
			static const std::string s2d("= \t");
			Tokenizer<BufferRef> st2(kv, s2d);
			BufferRef key(st2.nextToken());
			BufferRef value(st2.nextToken());
			//printf("parsed cookie[%s] = '%s'\n", key.c_str(), value.c_str());
			if (key == wanted) {
				args.setResult(value);
				return;
			}
			//cookies_[key] = value;
		}
	}
	args.setResult("");
}

void XzeroCore::req_host(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->hostname);
}

void XzeroCore::req_pathinfo(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->pathinfo);
}

void XzeroCore::req_is_secure(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->connection.isSecure());
}

void XzeroCore::req_scheme(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->connection.isSecure() ? "https" : "http");
}

void XzeroCore::req_status_code(HttpRequest* in, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(in->status));
}
// }}}
// {{{ connection
void XzeroCore::conn_remote_ip(HttpRequest* in, FlowParams& args)
{
	args.setResult(&in->connection.remoteIP());
}

void XzeroCore::conn_remote_port(HttpRequest* in, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(in->connection.remotePort()));
}

void XzeroCore::conn_local_ip(HttpRequest* in, FlowParams& args)
{
	args.setResult(&in->connection.localIP());
}

void XzeroCore::conn_local_port(HttpRequest* in, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(in->connection.localPort()));
}
// }}}
// {{{ phys
void XzeroCore::phys_path(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->path().c_str() : "");
}

void XzeroCore::phys_exists(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->exists() : false);
}

void XzeroCore::phys_is_reg(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->isRegular() : false);
}

void XzeroCore::phys_is_dir(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->isDirectory() : false);
}

void XzeroCore::phys_is_exe(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->isExecutable() : false);
}

void XzeroCore::phys_mtime(HttpRequest* in, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(in->fileinfo ? in->fileinfo->mtime() : 0));
}

void XzeroCore::phys_size(HttpRequest* in, FlowParams& args)
{
	args.setResult(static_cast<FlowNumber>(in->fileinfo ? in->fileinfo->size() : 0));
}

void XzeroCore::phys_etag(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->etag().c_str() : "");
}

void XzeroCore::phys_mimetype(HttpRequest* in, FlowParams& args)
{
	args.setResult(in->fileinfo ? in->fileinfo->mimetype().c_str() : "");
}
// }}}
// {{{ regex
void XzeroCore::regex_group(HttpRequest* in, FlowParams& args)
{
	FlowNumber position = args.get<FlowNumber>(1);

	if (const RegExp::Result* rr = in->regexMatch()) {
		if (position >= 0 && position < static_cast<FlowNumber>(rr->size())) {
			const auto& match = rr->at(position);
            FlowString result(match.first, match.second);
			args.setResult(args.caller()->newString(match.first, match.second));
		} else {
			// match index out of bounds
			args.setResult("");
		}
	} else {
		// no regex match executed
		args.setResult("");
	}
}
// }}}
// {{{ file
// bool file.exists(string path)
void XzeroCore::file_exists(HttpRequest* in, FlowParams& args)
{
	auto fileinfo = in->connection.worker().fileinfo(args.get<FlowString>(1).str());
	if (!fileinfo)
        args.setResult(false);
    else
        args.setResult(fileinfo->exists());
}

void XzeroCore::file_is_reg(HttpRequest* in, FlowParams& args)
{
    HttpWorker* worker = in ? &in->connection.worker() : server().mainWorker();
    auto fileinfo = worker->fileinfo(args.get<FlowString>(1).str());
    if (!fileinfo)
        args.setResult(false);
    else
        args.setResult(fileinfo->isRegular());
}

void XzeroCore::file_is_dir(HttpRequest* in, FlowParams& args)
{
	HttpWorker* worker = in ? &in->connection.worker() : server().mainWorker();
	auto fileinfo = worker->fileinfo(args.get<FlowString>(1).str());
	if (!fileinfo)
        args.setResult(false);
    else
        args.setResult(fileinfo->isDirectory());
}

void XzeroCore::file_is_exe(HttpRequest* in, FlowParams& args)
{
	HttpWorker* worker = in ? &in->connection.worker() : server().mainWorker();
	auto fileinfo = worker->fileinfo(args.get<FlowString>(1).str());
	if (!fileinfo)
        args.setResult(false);
    else
        args.setResult(fileinfo->isExecutable());
}
// }}}
// {{{ handler
bool XzeroCore::redirect(HttpRequest *in, FlowParams& args)
{
	in->status = HttpStatus::MovedTemporarily;
	in->responseHeaders.overwrite("Location", args.get<FlowString>(1).str());
	in->finish();

	return true;
}

bool XzeroCore::respond(HttpRequest *in, FlowParams& args)
{
    in->status = static_cast<HttpStatus>(args.get<FlowNumber>(1));
	in->finish();

	return true;
}

bool XzeroCore::echo(HttpRequest *in, FlowParams& args)
{
    in->write<x0::BufferSource>(args.get<FlowString>(1).str());

	// trailing newline
	in->write<x0::BufferSource>("\n");

	if (!in->status)
		in->status = HttpStatus::Ok;

	in->finish();
	return true;
}

bool XzeroCore::blank(HttpRequest* in, FlowParams& args)
{
	in->status = HttpStatus::Ok;
	in->finish();

	return true;
}
// }}}
// {{{ response's header.* functions
void XzeroCore::header_add(HttpRequest* r, FlowParams& args)
{
    r->responseHeaders.push_back(args.get<FlowString>(1).str(), args.get<FlowString>(2).str());
}

// header.append(headerName, appendValue)
void XzeroCore::header_append(HttpRequest* r, FlowParams& args)
{
    r->responseHeaders[args.get<FlowString>(1).str()] += args.get<FlowString>(2).str();
}

void XzeroCore::header_overwrite(HttpRequest* r, FlowParams& args)
{
    r->responseHeaders.overwrite(args.get<FlowString>(1).str(), args.get<FlowString>(2).str());
}

void XzeroCore::header_remove(HttpRequest* r, FlowParams& args)
{
    r->responseHeaders.remove(args.get<FlowString>(1).str());
}
// }}}
bool XzeroCore::staticfile(HttpRequest *in, FlowParams& args) // {{{
{
	if (!in->fileinfo)
		return false;

	if (!in->fileinfo->exists())
		return false;

	if (in->testDirectoryTraversal())
		return true;

	if (!in->fileinfo->isRegular())
		return false;

	if (in->status != HttpStatus::Undefined) {
		// processing an internal redirect (to a static file)

		in->responseHeaders.push_back("Last-Modified", in->fileinfo->lastModified());
		in->responseHeaders.push_back("ETag", in->fileinfo->etag());

		in->responseHeaders.push_back("Content-Type", in->fileinfo->mimetype());
		in->responseHeaders.push_back("Content-Length", lexical_cast<std::string>(in->fileinfo->size()));

		int fd = in->fileinfo->handle();
		if (fd < 0) {
			log(Severity::error, "Could not open file: '%s': %s", in->fileinfo->filename().c_str(), strerror(errno));
			return false;
		}

		posix_fadvise(fd, 0, in->fileinfo->size(), POSIX_FADV_SEQUENTIAL);
		in->write<FileSource>(fd, 0, in->fileinfo->size(), false);
		in->finish();

		return true;
	}

	if (in->sendfile(in->fileinfo)) {
		in->finish();
		return true;
	}
	return false;
} // }}}
// {{{ handler: precompressed
bool XzeroCore::precompressed(HttpRequest *in, FlowParams& args)
{
	if (!in->fileinfo)
		return false;

	if (!in->fileinfo->exists())
		return false;

	if (in->testDirectoryTraversal())
		return true;

	if (!in->fileinfo->isRegular())
		return false;

	if (BufferRef r = in->requestHeader("Accept-Encoding")) {
		auto items = Tokenizer<BufferRef>::tokenize(r, ", ");

		static const struct {
			const char* id;
			const char* fileExtension;
		} encodings[] = {
			{ "gzip", ".gz" },
			{ "bzip2", ".bz2" },
//			{ "lzma", ".lzma" },
		};

		for (auto& encoding: encodings) {
			if (std::find(items.begin(), items.end(), encoding.id) != items.end()) {
				auto pc = in->connection.worker().fileinfo(in->fileinfo->path() + encoding.fileExtension);

				if (pc->exists() && pc->isRegular() && pc->mtime() == in->fileinfo->mtime()) {
					in->responseHeaders.push_back("Content-Encoding", encoding.id);
					return in->sendfile(pc);
				}
			}
		}
	}

	return false;
} // }}}
// {{{ post_config
bool XzeroCore::post_config()
{
    if (emitIR_) {
        // XXX we could certainly dump this into a special destination instead to stderr.
        daemon().program_->dump();
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

unsigned long long XzeroCore::getrlimit(int resource)
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

unsigned long long XzeroCore::setrlimit(int resource, unsigned long long value)
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
bool XzeroCore::redirectOnIncompletePath(HttpRequest *in)
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
	in->status = HttpStatus::MovedPermanently;

	in->finish();
	return true;
}

} // namespace x0d
