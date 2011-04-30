/* <HttpCore.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_HttpCore_h
#define sw_x0_HttpCore_h 1

#include <x0/Api.h>
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpError.h>

namespace x0 {

class FlowValue;

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
	void user(FlowValue& result, const Params& args);
	bool drop_privileges(const std::string& username, const std::string& groupname);
	void plugin_directory(FlowValue& result, const Params& args);
	void mimetypes(FlowValue& result, const Params& args);
	void mimetypes_default(FlowValue& result, const Params& args);
	void etag_mtime(FlowValue& result, const Params& args);
	void etag_size(FlowValue& result, const Params& args);
	void etag_inode(FlowValue& result, const Params& args);
	void fileinfo_cache_ttl(FlowValue& result, const Params& args);
	void server_advertise(FlowValue& result, const Params& args);
	void server_tags(FlowValue& result, const Params& args);
	void loadServerTag(const FlowValue& tag);

	void max_read_idle(FlowValue& result, const Params& args);
	void max_write_idle(FlowValue& result, const Params& args);
	void max_keepalive_idle(FlowValue& result, const Params& args);
	void max_conns(FlowValue& result, const Params& args);
	void max_files(FlowValue& result, const Params& args);
	void max_address_space(FlowValue& result, const Params& args);
	void max_core(FlowValue& result, const Params& args);
	void tcp_cork(FlowValue& result, const Params& args);
	void tcp_nodelay(FlowValue& result, const Params& args);

	// debugging
	void emit_llvm(FlowValue& result, const Params& args);

	// logging
	void loglevel(FlowValue& result, const Params& args);
	void logfile(FlowValue& result, const Params& args);
	void log_sd(FlowValue& result, const Params& args);

	// setup
	void listen(FlowValue& result, const Params& args);
	void workers(FlowValue& result, const Params& args);

	// shared methods
	void sys_env(FlowValue& result, const Params& args);
	void sys_cwd(FlowValue& result, const Params& args);
	void sys_pid(FlowValue& result, const Params& args);
	void sys_now(FlowValue& result, const Params& args);
	void sys_now_str(FlowValue& result, const Params& args);

	// main methods
	void autoindex(FlowValue& result, HttpRequest *r, const Params& args);
	bool matchIndex(HttpRequest *in, const FlowValue& arg);
	bool docroot(HttpRequest *r, const Params& args);
	bool alias(HttpRequest *r, const Params& args);
	void pathinfo(FlowValue& result, HttpRequest *r, const Params& args);
	void error_handler(FlowValue& result, HttpRequest *r, const Params& args);
	void req_method(FlowValue& result, HttpRequest *r, const Params& args);
	void req_url(FlowValue& result, HttpRequest *r, const Params& args);
	void req_path(FlowValue& result, HttpRequest *r, const Params& args);
	void req_header(FlowValue& result, HttpRequest *r, const Params& args);
	void req_host(FlowValue& result, HttpRequest *r, const Params& args);
	void req_pathinfo(FlowValue& result, HttpRequest *r, const Params& args);
	void req_is_secure(FlowValue& result, HttpRequest *r, const Params& args);
	void req_status_code(FlowValue& result, HttpRequest *r, const Params& args);
	void req_rewrite(FlowValue& result, HttpRequest *r, const Params& args);
	void conn_remote_ip(FlowValue& result, HttpRequest *r, const Params& args);
	void conn_remote_port(FlowValue& result, HttpRequest *r, const Params& args);
	void conn_local_ip(FlowValue& result, HttpRequest *r, const Params& args);
	void conn_local_port(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_path(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_exists(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_is_reg(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_is_dir(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_is_exe(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_mtime(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_size(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_etag(FlowValue& result, HttpRequest *r, const Params& args);
	void phys_mimetype(FlowValue& result, HttpRequest *r, const Params& args);
	void header_add(FlowValue& result, HttpRequest *r, const Params& args);
	void header_append(FlowValue& result, HttpRequest *r, const Params& args);
	void header_overwrite(FlowValue& result, HttpRequest *r, const Params& args);
	void header_remove(FlowValue& result, HttpRequest *r, const Params& args);

	// main handlers
	bool redirect(HttpRequest *r, const Params& args);
	bool respond(HttpRequest *r, const Params& args);

	bool staticfile(HttpRequest *r, const Params& args);
	inline HttpError verifyClientCache(HttpRequest *r);
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
