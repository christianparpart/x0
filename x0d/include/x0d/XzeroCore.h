/* <XzeroCore.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_XzeroCore_h
#define sw_x0_XzeroCore_h 1

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpStatus.h>
#include <x0/flow/FlowValue.h>
#include <x0/Api.h>

namespace x0d {

//! \addtogroup http
//@{

class XzeroDaemon;

class X0_API XzeroCore :
	public XzeroPlugin
{
private:
	bool emitLLVM_;

public:
	explicit XzeroCore(XzeroDaemon* d);
	~XzeroCore();

	x0::Property<unsigned long long> max_fds;

	virtual bool post_config();

	bool redirectOnIncompletePath(x0::HttpRequest *r);

private:
	// setup properties
	void plugin_directory(const x0::FlowParams& args, x0::FlowValue& result);
	void mimetypes(const x0::FlowParams& args, x0::FlowValue& result);
	void mimetypes_default(const x0::FlowParams& args, x0::FlowValue& result);
	void etag_mtime(const x0::FlowParams& args, x0::FlowValue& result);
	void etag_size(const x0::FlowParams& args, x0::FlowValue& result);
	void etag_inode(const x0::FlowParams& args, x0::FlowValue& result);
	void fileinfo_cache_ttl(const x0::FlowParams& args, x0::FlowValue& result);
	void server_advertise(const x0::FlowParams& args, x0::FlowValue& result);
	void server_tags(const x0::FlowParams& args, x0::FlowValue& result);
	void loadServerTag(const x0::FlowValue& tag);

	void max_read_idle(const x0::FlowParams& args, x0::FlowValue& result);
	void max_write_idle(const x0::FlowParams& args, x0::FlowValue& result);
	void max_keepalive_idle(const x0::FlowParams& args, x0::FlowValue& result);
	void max_keepalive_requests(const x0::FlowParams& args, x0::FlowValue& result);
	void max_conns(const x0::FlowParams& args, x0::FlowValue& result);
	void max_files(const x0::FlowParams& args, x0::FlowValue& result);
	void max_address_space(const x0::FlowParams& args, x0::FlowValue& result);
	void max_core(const x0::FlowParams& args, x0::FlowValue& result);
	void tcp_cork(const x0::FlowParams& args, x0::FlowValue& result);
	void tcp_nodelay(const x0::FlowParams& args, x0::FlowValue& result);
	void lingering(const x0::FlowParams& args, x0::FlowValue& result);
	void max_request_uri_size(const x0::FlowParams& args, x0::FlowValue& result);
	void max_request_header_size(const x0::FlowParams& args, x0::FlowValue& result);
	void max_request_header_count(const x0::FlowParams& args, x0::FlowValue& result);
	void max_request_body_size(const x0::FlowParams& args, x0::FlowValue& result);

	// debugging
	void emit_llvm(const x0::FlowParams& args, x0::FlowValue& result);

	// setup functions
	void listen(const x0::FlowParams& args, x0::FlowValue& result);
	void workers(const x0::FlowParams& args, x0::FlowValue& result);

	// shared properties
	void systemd_controlled(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void systemd_booted(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_env(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_cwd(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_pid(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_now(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_now_str(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	// shared functions
	void log_err(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_warn(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_notice(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_info(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_debug(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	void file_exists(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void file_is_reg(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void file_is_dir(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void file_is_exe(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	// main handlers
	bool docroot(x0::HttpRequest* r, const x0::FlowParams& args);
	bool alias(x0::HttpRequest* r, const x0::FlowParams& args);
	bool redirect(x0::HttpRequest *r, const x0::FlowParams& args);
	bool respond(x0::HttpRequest *r, const x0::FlowParams& args);
	bool echo(x0::HttpRequest *r, const x0::FlowParams& args);
	bool blank(x0::HttpRequest *r, const x0::FlowParams& args);
	bool staticfile(x0::HttpRequest *r, const x0::FlowParams& args);
	bool precompressed(x0::HttpRequest *r, const x0::FlowParams& args);

	// main functions
	void autoindex(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	bool matchIndex(x0::HttpRequest *r, const x0::FlowValue& arg);
	void rewrite(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void pathinfo(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void error_handler(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_add(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_append(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_overwrite(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_remove(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	// main properties
	void req_method(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_url(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_path(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_query(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_header(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_cookie(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_host(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_pathinfo(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_is_secure(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_scheme(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_status_code(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_remote_ip(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_remote_port(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_local_ip(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_local_port(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_path(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_exists(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_is_reg(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_is_dir(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_is_exe(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_mtime(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_size(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_etag(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_mimetype(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void regex_group(x0::HttpRequest* in, const x0::FlowParams& args, x0::FlowValue& result);

	// {{{ legacy
private:
	unsigned long long getrlimit(int resource);
	unsigned long long setrlimit(int resource, unsigned long long max);
	// }}}
};

//@}

} // namespace x0

#endif
