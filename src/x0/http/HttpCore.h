/* <HttpCore.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_HttpCore_h
#define sw_x0_HttpCore_h 1

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpError.h>

namespace x0 {

class HttpServer;
class SettingsValue;
class Scope;

class HttpCore :
	public HttpPlugin
{
private:
	bool emitLLVM_;

public:
	explicit HttpCore(HttpServer& server);
	~HttpCore();

	Property<unsigned long long> max_fds;

	virtual bool post_config();

private:
	// setup properties
	void plugin_directory(Flow::Value& result, const Params& args);
	void mimetypes(Flow::Value& result, const Params& args);
	void mimetypes_default(Flow::Value& result, const Params& args);
	void etag_mtime(Flow::Value& result, const Params& args);
	void etag_size(Flow::Value& result, const Params& args);
	void etag_inode(Flow::Value& result, const Params& args);
	void server_advertise(Flow::Value& result, const Params& args);
	void server_tags(Flow::Value& result, const Params& args);
	void loadServerTag(const Flow::Value& tag);

	void max_read_idle(Flow::Value& result, const Params& args);
	void max_write_idle(Flow::Value& result, const Params& args);
	void max_keepalive_idle(Flow::Value& result, const Params& args);
	void max_conns(Flow::Value& result, const Params& args);
	void max_files(Flow::Value& result, const Params& args);
	void max_address_space(Flow::Value& result, const Params& args);
	void max_core(Flow::Value& result, const Params& args);
	void tcp_cork(Flow::Value& result, const Params& args);
	void tcp_nodelay(Flow::Value& result, const Params& args);

	// setup methods
	void plugin_load(Flow::Value& result, const Params& args);
	void listen(Flow::Value& result, const Params& args);

	// debugging
	void emit_llvm(Flow::Value& result, const Params& args);

	// shared methods
	void sys_env(Flow::Value& result, const Params& args);
	void sys_cwd(Flow::Value& result, const Params& args);
	void sys_pid(Flow::Value& result, const Params& args);
	void sys_now(Flow::Value& result, const Params& args);
	void sys_now_str(Flow::Value& result, const Params& args);

	// main methods
	void autoindex(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	bool matchIndex(HttpRequest *in, const Flow::Value& arg);
	void docroot(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void alias(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void pathinfo(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void req_method(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void req_url(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void req_path(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void req_header(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void req_host(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void req_pathinfo(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void resp_header_add(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void resp_header_overwrite(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void resp_header_append(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void resp_header_remove(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void conn_remote_ip(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void conn_remote_port(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void conn_local_ip(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void conn_local_port(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_path(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_exists(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_is_reg(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_is_dir(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_is_exe(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_mtime(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_size(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_etag(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);
	void phys_mimetype(Flow::Value& result, HttpRequest *in, HttpResponse *out, const Params& args);

	// main handlers
	bool dirlisting(HttpRequest *in, HttpResponse *out, const Params& args);
	bool redirect(HttpRequest *in, HttpResponse *out, const Params& args);
	bool respond(HttpRequest *in, HttpResponse *out, const Params& args);

	bool staticfile(HttpRequest *in, HttpResponse *out, const Params& args);
	inline http_error verifyClientCache(HttpRequest *in, HttpResponse *out);
	inline bool processRangeRequest(HttpRequest *in, HttpResponse *out, int fd);
	inline std::pair<std::size_t, std::size_t> makeOffsets(const std::pair<std::size_t, std::size_t>& p, std::size_t actual_size);
	inline std::string generateBoundaryID() const;

	// {{{ legacy
private:
	long long getrlimit(int resource);
	long long setrlimit(int resource, long long max);

	std::error_code setupMimeTypeDefault(const SettingsValue& cvar, Scope& s);

	std::error_code setupETagUseMtime(const SettingsValue& cvar, Scope& s);
	std::error_code setupETagUseSize(const SettingsValue& cvar, Scope& s);
	std::error_code setupETagUseInode(const SettingsValue& cvar, Scope& s);

	std::error_code setup_error_documents(const SettingsValue& cvar, Scope& s);
	// }}}
};

} // namespace x0

#endif
