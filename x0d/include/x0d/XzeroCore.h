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

	Property<unsigned long long> max_fds;

	virtual bool post_config();

	bool redirectOnIncompletePath(HttpRequest *r);

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
	void systemd_controlled(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void systemd_booted(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_env(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_cwd(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_pid(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_now(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void sys_now_str(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	// shared functions
	void log_err(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_warn(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_notice(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_info(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void log_debug(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	void file_exists(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void file_is_reg(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void file_is_dir(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void file_is_exe(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	// main handlers
	bool docroot(HttpRequest* r, const x0::FlowParams& args);
	bool alias(HttpRequest* r, const x0::FlowParams& args);
	bool redirect(HttpRequest *r, const x0::FlowParams& args);
	bool respond(HttpRequest *r, const x0::FlowParams& args);
	bool echo(HttpRequest *r, const x0::FlowParams& args);
	bool blank(HttpRequest *r, const x0::FlowParams& args);
	bool staticfile(HttpRequest *r, const x0::FlowParams& args);
	bool precompressed(HttpRequest *r, const x0::FlowParams& args);

	// main functions
	void autoindex(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	bool matchIndex(HttpRequest *r, const x0::FlowValue& arg);
	void rewrite(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void pathinfo(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void error_handler(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_add(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_append(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_overwrite(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void header_remove(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);

	// main properties
	void req_method(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_url(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_path(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_query(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_header(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_cookie(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_host(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_pathinfo(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_is_secure(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_scheme(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void req_status_code(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_remote_ip(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_remote_port(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_local_ip(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void conn_local_port(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_path(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_exists(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_is_reg(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_is_dir(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_is_exe(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_mtime(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_size(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_etag(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void phys_mimetype(HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result);
	void regex_group(HttpRequest* in, const x0::FlowParams& args, x0::FlowValue& result);

	// {{{ legacy
private:
	unsigned long long getrlimit(int resource);
	unsigned long long setrlimit(int resource, unsigned long long max);
	// }}}
};

//@}

} // namespace x0

#endif
