/* <HttpCore.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_HttpCore_h
#define sw_x0_HttpCore_h 1

#include <x0/Api.h>
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpStatus.h>
#include <x0/flow/FlowValue.h>

namespace x0 {

//! \addtogroup http
//@{

class HttpServer;

class X0_API HttpCore :
	public HttpPlugin
{
private:
	bool emitLLVM_;

public:
	explicit HttpCore(HttpServer& server);
	~HttpCore();

	Property<unsigned long long> max_fds;

	virtual bool post_config();

	bool redirectOnIncompletePath(HttpRequest *r);

private:
	// setup properties
	void plugin_directory(const FlowParams& args, FlowValue& result);
	void mimetypes(const FlowParams& args, FlowValue& result);
	void mimetypes_default(const FlowParams& args, FlowValue& result);
	void etag_mtime(const FlowParams& args, FlowValue& result);
	void etag_size(const FlowParams& args, FlowValue& result);
	void etag_inode(const FlowParams& args, FlowValue& result);
	void fileinfo_cache_ttl(const FlowParams& args, FlowValue& result);
	void server_advertise(const FlowParams& args, FlowValue& result);
	void server_tags(const FlowParams& args, FlowValue& result);
	void loadServerTag(const FlowValue& tag);

	void max_read_idle(const FlowParams& args, FlowValue& result);
	void max_write_idle(const FlowParams& args, FlowValue& result);
	void max_keepalive_idle(const FlowParams& args, FlowValue& result);
	void max_keepalive_requests(const FlowParams& args, FlowValue& result);
	void max_conns(const FlowParams& args, FlowValue& result);
	void max_files(const FlowParams& args, FlowValue& result);
	void max_address_space(const FlowParams& args, FlowValue& result);
	void max_core(const FlowParams& args, FlowValue& result);
	void tcp_cork(const FlowParams& args, FlowValue& result);
	void tcp_nodelay(const FlowParams& args, FlowValue& result);
	void max_request_uri_size(const FlowParams& args, FlowValue& result);
	void max_request_header_size(const FlowParams& args, FlowValue& result);
	void max_request_header_count(const FlowParams& args, FlowValue& result);
	void max_request_body_size(const FlowParams& args, FlowValue& result);

	// debugging
	void emit_llvm(const FlowParams& args, FlowValue& result);

	// setup functions
	void listen(const FlowParams& args, FlowValue& result);
	void workers(const FlowParams& args, FlowValue& result);

	// shared properties
	void systemd_controlled(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void systemd_booted(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void sys_env(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void sys_cwd(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void sys_pid(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void sys_now(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void sys_now_str(HttpRequest* r, const FlowParams& args, FlowValue& result);

	// shared functions
	void log_err(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void log_info(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void log_debug(HttpRequest* r, const FlowParams& args, FlowValue& result);

	// main handlers
	bool docroot(HttpRequest* r, const FlowParams& args);
	bool alias(HttpRequest* r, const FlowParams& args);
	bool redirect(HttpRequest *r, const FlowParams& args);
	bool respond(HttpRequest *r, const FlowParams& args);
	bool blank(HttpRequest *r, const FlowParams& args);
	bool staticfile(HttpRequest *r, const FlowParams& args);
	bool precompressed(HttpRequest *r, const FlowParams& args);

	// main functions
	void autoindex(HttpRequest* r, const FlowParams& args, FlowValue& result);
	bool matchIndex(HttpRequest *r, const FlowValue& arg);
	void rewrite(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void pathinfo(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void error_handler(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void header_add(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void header_append(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void header_overwrite(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void header_remove(HttpRequest* r, const FlowParams& args, FlowValue& result);

	// main properties
	void req_method(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_url(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_path(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_header(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_cookie(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_host(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_pathinfo(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_is_secure(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_scheme(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void req_status_code(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void conn_remote_ip(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void conn_remote_port(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void conn_local_ip(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void conn_local_port(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_path(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_exists(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_is_reg(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_is_dir(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_is_exe(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_mtime(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_size(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_etag(HttpRequest* r, const FlowParams& args, FlowValue& result);
	void phys_mimetype(HttpRequest* r, const FlowParams& args, FlowValue& result);

	// helpers
	inline HttpStatus verifyClientCache(HttpRequest *r);
	inline bool processStaticFile(HttpRequest *r, FileInfoPtr transferFile);
	inline bool processRangeRequest(HttpRequest *r, int fd);
	inline std::pair<std::size_t, std::size_t> makeOffsets(const std::pair<std::size_t, std::size_t>& p, std::size_t actual_size);
	inline std::string generateBoundaryID() const;

	// {{{ legacy
private:
	unsigned long long getrlimit(int resource);
	unsigned long long setrlimit(int resource, unsigned long long max);
	// }}}
};

//@}

} // namespace x0

#endif
