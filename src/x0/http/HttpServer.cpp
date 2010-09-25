/* <x0/HttpServer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpServer.h>
#include <x0/http/HttpListener.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpCore.h>
#include <x0/Settings.h>
#include <x0/Error.h>
#include <x0/Logger.h>
#include <x0/Library.h>
#include <x0/AnsiColor.h>
#include <x0/strutils.h>
#include <x0/sysconfig.h>

#include <flow/flow.h>
#include <flow/value.h>
#include <flow/parser.h>
#include <flow/runner.h>
#include <flow/backend.h>

#include <iostream>
#include <cstdarg>
#include <cstdlib>

#if defined(HAVE_SYS_UTSNAME_H)
#	include <sys/utsname.h>
#endif

#include <sys/resource.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>

#define TRACE(level, msg...) debug(level, msg)

namespace x0 {

/** initializes the HTTP server object.
 * \param io_service an Asio io_service to use or NULL to create our own one.
 * \see HttpServer::run()
 */
HttpServer::HttpServer(struct ::ev_loop *loop) :
	Scope("server"),
	onConnectionOpen(),
	onPreProcess(),
	onResolveDocumentRoot(),
	onResolveEntity(),
	onPostProcess(),
	onRequestDone(),
	onConnectionClose(),
	components_(),
	vhosts_(),

	runner_(NULL),
	onHandleRequest_(),
	in_(NULL),
	out_(NULL),

	listeners_(),
	loop_(loop ? loop : ev_default_loop(0)),
	active_(false),
	settings_(),
	cvars_server_(),
	cvars_host_(),
	cvars_path_(),
	logger_(),
	logLevel_(Severity::warn),
	colored_log_(false),
	pluginDirectory_(PLUGINDIR),
	plugins_(),
	pluginLibraries_(),
	now_(),
	loop_check_(loop_),
	core_(0),
	max_connections(512),
	max_keep_alive_idle(/*5*/ 60),
	max_read_idle(60),
	max_write_idle(360),
	tcp_cork(false),
	tcp_nodelay(false),
	tag("x0/" VERSION),
	advertise(true),
	fileinfo(loop_)
{
	HttpResponse::initialize();

	// initialize all cvar maps with all (valid) priorities
	for (int i = -10; i <= +10; ++i)
	{
		cvars_server_[i].clear();
		cvars_host_[i].clear();
		cvars_path_[i].clear();
	}

	loop_check_.set<HttpServer, &HttpServer::loop_check>(this);
	loop_check_.start();
}

void HttpServer::loop_check(ev::check& /*w*/, int /*revents*/)
{
	// update server time
	now_.update(static_cast<time_t>(ev_now(loop_)));
}

HttpServer::~HttpServer()
{
	stop();

	for (std::list<HttpListener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		delete *k;

	unregisterPlugin(core_);
	delete core_;
	core_ = 0;

	while (!plugins_.empty())
		unloadPlugin(plugins_[plugins_.size() - 1]->name());
}

/** tests whether given cvar-token is available in the table of registered cvars. */
inline bool _contains(const std::map<int, std::map<std::string, cvar_handler>>& map, const std::string& cvar)
{
	for (auto pi = map.begin(), pe = map.end(); pi != pe; ++pi)
		for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			if (ci->first == cvar)
				return true;

	return false;
}

inline bool _contains(const std::vector<std::string>& list, const std::string& var)
{
	for (auto i = list.begin(), e = list.end(); i != e; ++i)
		if (*i == var)
			return true;

	return false;
}

#if 0
/**
 * configures the server ready to be started.
 */
bool HttpServer::configure(const std::string& configfile)
{
	std::vector<std::string> global_ignores = {
		"IGNORES",
		"string", "xpcall", "package", "io", "coroutine", "collectgarbage", "getmetatable", "module",
		"loadstring", "rawget", "rawset", "ipairs", "pairs", "_G", "next", "assert", "tonumber",
		"rawequal", "tostring", "print", "os", "unpack", "gcinfo", "require", "getfenv", "setmetatable",
		"type", "newproxy", "table", "pcall", "math", "debug", "select", "_VERSION",
		"dofile", "setfenv", "load", "error", "loadfile"
	};

	if (!core_)
		registerPlugin(core_ = new HttpCore(*this));

	// load config
	std::error_code ec = settings_.load_file(configfile);
	if (ec) {
		log(Severity::error, "Could not load configuration file: %s", ec.message().c_str());
		return false;
	}

	ec = settings_.load("Plugins.Directory", pluginDirectory_);
	if (ec) {
		log(Severity::error, "Could not get plugin directory: %s", ec.message().c_str());
		return false;
	}

	// {{{ global vars
	auto globals = settings_.keys();
	auto custom_ignores = settings_["IGNORES"].values<std::string>();

	// iterate through all server cvars and evaluate them (if found in config file)
	for (auto pi = cvars_server_.begin(), pe = cvars_server_.end(); pi != pe; ++pi) {
		for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci) {
			if (settings_.contains(ci->first)) {
				std::error_code ec = ci->second(settings_[ci->first], *this);
				if (ec) {
					log(Severity::debug, "Evaluating global configuration variable '%s' failed: %s", ci->first.c_str(), ec.message().c_str());
					return false;
				}
			}
		}
	}
	// }}}

	// {{{ warn on every unknown global cvar
	for (auto i = globals.begin(), e = globals.end(); i != e; ++i)
	{
		if (_contains(global_ignores, *i))
			continue;

		if (_contains(custom_ignores, *i))
			continue;

		if (!_contains(cvars_server_, *i))
		{
			log(Severity::error, "Unknown global configuration variable: '%s'.", i->c_str());
			return false;
		}
	} // }}}

	// {{{ warn on every unknown vhost cvar
	for (auto i = vhosts_.begin(), e = vhosts_.end(); i != e; ++i)
	{
		if (i->first != i->second->id())
			continue; // skip aliases

		auto keys = settings_["Hosts"][i->first].keys<std::string>();
		for (auto k = keys.begin(), m = keys.end(); k != m; ++k) {
			if (!_contains(cvars_host_, *k)) {
				log(Severity::error, "Unknown virtual-host configuration variable: '%s'.", k->c_str());
				return false;
			}
		}
	} // }}}

	// {{{ merge settings scopes (server to vhost)
	for (auto i = vhosts_.begin(), e = vhosts_.end(); i != e; ++i)
	{
		if (i->first != i->second->id())
			continue; // skip aliases

		i->second->merge(this);
	} // }}}

	// {{{ run post-config hooks
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		if (!(*i)->post_config())
			return false;
	// }}}

	// {{{ run post-check hooks
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		if (!(*i)->post_check())
			return false;
	// }}}

	// {{{ setup server-tag
	{
		settings_.load<std::vector<std::string>>("ServerTags", components_);

		//! \todo add zlib version
		//! \todo add bzip2 version

#if defined(HAVE_SYS_UTSNAME_H)
		{
			utsname utsname;
			if (uname(&utsname) == 0)
			{
				components_.insert(components_.begin(), 
					std::string(utsname.sysname) + "/" + utsname.release
				);

				components_.insert(components_.begin(), utsname.machine);
			}
		}
#endif

		Buffer tagbuf;
		tagbuf.push_back("x0/" VERSION);

		if (!components_.empty())
		{
			tagbuf.push_back(" (");

			for (int i = 0, e = components_.size(); i != e; ++i)
			{
				if (i)
					tagbuf.push_back(", ");

				tagbuf.push_back(components_[i]);
			}

			tagbuf.push_back(")");
		}
		tag = tagbuf.str();
	}
	// }}}

	// {{{ setup workers
#if 0
	{
		int num_workers = 1;
		settings_.load("Resources.NumWorkers", num_workers);
		//io_service_pool_.setup(num_workers);

		if (num_workers > 1)
			log(Severity::info, "using %d workers", num_workers);
		else
			log(Severity::info, "using single worker");
	}
#endif
	// }}}

	// {{{ check for available TCP listeners
	if (listeners_.empty())
	{
		log(Severity::error, "No HTTP listeners defined");
		return false;
	}

	for (auto i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
		if (!(*i)->prepare())
			return false;
	// }}}

	// {{{ setup process priority
	if (int nice_ = settings_.get<int>("Daemon.Nice"))
	{
		debug(1, "set nice level to %d", nice_);

		if (::nice(nice_) < 0)
			log(Severity::error, "could not nice process to %d: %s", nice_, strerror(errno));
	}
	// }}}

	return true;
}
#endif
void wrap_log_parser_error(HttpServer *srv, const char *cat, const std::string& msg)
{
	printf("%s: %s\n", cat, msg.c_str());
	srv->log(Severity::error, "%s: %s", cat, msg.c_str());
}

bool HttpServer::setup(const std::string& configFile)
{
	Flow::Parser parser;
	parser.setErrorHandler(std::bind(&wrap_log_parser_error, this, "parser", std::placeholders::_1));
	if (!parser.open(configFile)) {
		perror("open");
		return false;
	}

	configfile_ = configFile;

	Flow::Unit *unit = parser.parse();
	if (!unit)
		return false;

	Flow::Function *fn = unit->lookup<Flow::Function>("setup");
	if (!fn) {
		printf("%s: no setup handler defined.\n", configFile.c_str());
		return false;
	}

	runner_ = new Flow::Runner(this);
	runner_->setErrorHandler(std::bind(&wrap_log_parser_error, this, "parser", std::placeholders::_1));

	// register setup API
	registerVariable("plugin.directory", Flow::Value::STRING, &HttpServer::flow_plugin_directory, this);
	registerHandler("plugin.load", &HttpServer::flow_plugin_load, this);
	registerHandler("listen", &HttpServer::flow_listen, this);
	registerHandler("group", &HttpServer::flow_group, this);
	registerHandler("user", &HttpServer::flow_user, this);
	registerVariable("mimetypes", Flow::Value::VOID, &HttpServer::flow_mimetypes, this);
	registerFunction("log", Flow::Value::VOID, &HttpServer::flow_log, this);
	registerFunction("sys.env", Flow::Value::STRING, &HttpServer::flow_sys_env, this);
	registerVariable("sys.cwd", Flow::Value::STRING, &HttpServer::flow_sys_cwd, this);
	registerVariable("sys.pid", Flow::Value::NUMBER, &HttpServer::flow_sys_pid, this);
	registerFunction("sys.now", Flow::Value::NUMBER, &HttpServer::flow_sys_now, this);
	registerFunction("sys.now_str", Flow::Value::STRING, &HttpServer::flow_sys_now_str, this);

	Flow::Runner::HandlerFunction setupFn = runner_->compile(fn);

	if (!setupFn || setupFn())
		return false;

	// flow: unregister setup API
	unregisterNative("mimetypes");
	unregisterNative("plugins");
	unregisterNative("listen");
	unregisterNative("group");
	unregisterNative("user");

	// flow: register main API
	// connection
	registerVariable("req.remoteip", Flow::Value::STRING, &HttpServer::flow_remote_ip, this);
	registerVariable("req.remoteport", Flow::Value::NUMBER, &HttpServer::flow_remote_port, this);
	registerVariable("req.localip", Flow::Value::STRING, &HttpServer::flow_local_ip, this);
	registerVariable("req.localport", Flow::Value::NUMBER, &HttpServer::flow_local_port, this);

	// request
	registerVariable("req.method", Flow::Value::BUFFER, &HttpServer::flow_req_method, this);
	registerVariable("req.uri", Flow::Value::BUFFER, &HttpServer::flow_req_url, this);
	registerVariable("req.path", Flow::Value::BUFFER, &HttpServer::flow_req_path, this);
	registerFunction("req.header", Flow::Value::STRING, &HttpServer::flow_req_header, this);
	registerVariable("req.host", Flow::Value::STRING, &HttpServer::flow_hostname, this);

	registerFunction("docroot", Flow::Value::STRING, &HttpServer::flow_req_docroot, this);

	// response
	registerHandler("respond", &HttpServer::flow_respond, this);
	registerHandler("redirect", &HttpServer::flow_redirect, this);
	registerFunction("header.add", Flow::Value::VOID, &HttpServer::flow_header_add, this);
	registerFunction("header.append", Flow::Value::VOID, &HttpServer::flow_header_append, this);
	registerFunction("header.overwrite", Flow::Value::VOID, &HttpServer::flow_header_overwrite, this);
	registerFunction("header.remove", Flow::Value::VOID, &HttpServer::flow_header_remove, this);

	// phys
	registerVariable("phys.exists", Flow::Value::BOOLEAN, &HttpServer::flow_phys_exists, this);
	registerVariable("phys.is_dir", Flow::Value::BOOLEAN, &HttpServer::flow_phys_is_dir, this);
	registerVariable("phys.is_reg", Flow::Value::BOOLEAN, &HttpServer::flow_phys_is_reg, this);
	registerVariable("phys.is_exe", Flow::Value::BOOLEAN, &HttpServer::flow_phys_is_exe, this);
	registerVariable("phys.mtime", Flow::Value::NUMBER, &HttpServer::flow_phys_mtime, this);
	registerVariable("phys.size", Flow::Value::NUMBER, &HttpServer::flow_phys_size, this);
	registerVariable("phys.etag", Flow::Value::STRING, &HttpServer::flow_phys_etag, this);
	registerVariable("phys.mimetype", Flow::Value::STRING, &HttpServer::flow_phys_mimetype, this);

	onHandleRequest_ = runner_->compile(unit->lookup<Flow::Function>("main"));
	if (!onHandleRequest_)
		return false;

	//runner_->dump();

	return true;
}

// {{{ flow: setup
void HttpServer::flow_plugin_directory(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	if (argc == 1)
		self->pluginDirectory_ = argv[1].toString();
	else if (argc == 0)
		argv[0].set(self->pluginDirectory_.c_str());
}

void HttpServer::flow_plugin_load(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	argv[0] = false;

	for (int i = 1; i <= argc; ++i)
	{
		if (!argv[i].isString())
			continue;

		const char *pluginName = argv[i].toString();
		std::error_code ec;
		self->loadPlugin(pluginName, ec);
		if (ec) {
			self->log(Severity::error, "%s: %s", pluginName, ec.message().c_str());
			argv[0].set(true);
			//break;
		}
	}
}

// write-only varable
void HttpServer::flow_mimetypes(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	if (argc == 1 && argv[1].isString())
	{
		self->fileinfo.load_mimetypes(argv[1].toString());
	}
}

void HttpServer::flow_listen(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	std::string arg(argv[1].toString());
	size_t n = arg.find(':');
	std::string ip = n != std::string::npos ? arg.substr(0, n) : "0.0.0.0";
	int port = atoi(n != std::string::npos ? arg.substr(n + 1).c_str() : arg.c_str());

	HttpListener *listener = self->setupListener(port, ip);
	if (listener != NULL) {
		argv[0] = false;
	} else {
		argv[0] = true;
	}
}

void HttpServer::flow_group(void *p, int argc, Flow::Value *argv)
{
//	HttpServer *self = (HttpServer *)p;
}

void HttpServer::flow_user(void *p, int argc, Flow::Value *argv)
{
//	HttpServer *self = (HttpServer *)p;
}
// }}}

// {{{ flow: general
void HttpServer::flow_sys_env(void *, int argc, Flow::Value *argv)
{
	argv[0] = getenv(argv[1].toString());
}

void HttpServer::flow_sys_cwd(void *, int argc, Flow::Value *argv)
{
	static char buf[1024];
	argv[0] = getcwd(buf, sizeof(buf));
}

void HttpServer::flow_sys_pid(void *, int argc, Flow::Value *argv)
{
	argv[0] = (long long) getpid();
}

void HttpServer::flow_sys_now(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	argv[0] = (long long) self->now_.unixtime();
}

void HttpServer::flow_sys_now_str(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	argv[0] = self->now_.http_str().c_str();
}
// }}}

// {{{ flow: helper
void HttpServer::flow_log(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	for (int i = 1; i <= argc; ++i)
	{
		if (i > 1)
			printf("\t");

		if (!self->printValue(argv[i])) {
			self->log(Severity::error, "flow_log error: unknown value type (%d) for arg %d", argv[i].type(), i);
			fflush(stderr);
		}
	}
	printf("\n");
}

bool HttpServer::printValue(const Flow::Value& value)
{
	fflush(stderr);
	switch (value.type_)
	{
		case Flow::Value::BOOLEAN:
			printf(value.toBool() ? "true" : "false");
			break;
		case Flow::Value::NUMBER:
			printf("%lld", value.toNumber());
			break;
		case Flow::Value::STRING:
			printf("%s", value.toString());
			break;
		case Flow::Value::BUFFER: {
			long long length = value.toNumber();
			const char *p = value.toString();
			std::string data(p, p + length);
			printf("\"%s\"", data.c_str());
			break;
		}
		case Flow::Value::ARRAY: {
			const Flow::Value *p = value.toArray();
			printf("(");
			for (int k = 0; p[k].type() != Flow::Value::VOID; ++k) {
				if (k) printf(", ");
				printValue(p[k]);
			}
			printf(")");

			break;
		}
		default:
			return false;
	}

	return true;
}
// }}}

// {{{ flow: main
// {{{ connection
void HttpServer::flow_remote_ip(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	argv[0] = self->in_->connection.remote_ip().c_str();
}

void HttpServer::flow_remote_port(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	argv[0] = self->in_->connection.remote_port();
}

void HttpServer::flow_local_ip(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	argv[0] = self->in_->connection.local_ip().c_str();
}

void HttpServer::flow_local_port(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	argv[0] = self->in_->connection.local_port();
}
// }}}

// {{{ request
// get request's document root
void HttpServer::flow_req_docroot(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	if (argc == 1)
	{
		in->document_root = argv[1].toString();
		in->fileinfo = self->fileinfo(in->document_root + in->path);
	}
	else
		argv[0].set(in->document_root.c_str());
}

void HttpServer::flow_req_method(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	argv[0] = strdup(self->in_->method.str().c_str()); // FIXME strdup = bad. fix string rep in flow to pascal-like strings!
}

// get request URL
void HttpServer::flow_req_url(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	argv[0].set(self->in_->uri.data(), self->in_->uri.size());
}

void HttpServer::flow_req_path(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	argv[0].set(self->in_->path.data(), self->in_->path.size());
}

// get request header
void HttpServer::flow_req_header(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	BufferRef ref(in->header(argv[1].toString()));
	argv[0].set(ref.data(), ref.size());
}

void HttpServer::flow_hostname(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	argv[0] = strdup(self->in_->hostname.str().c_str());
}
// }}}

// {{{ response
// handler: write response, with args: (int code);
void HttpServer::flow_respond(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;

	if (argc >= 1 && argv[1].isNumber())
		self->out_->status = static_cast<http_error>(argv[1].toNumber());

	self->out_->finish();
	argv[0] = true;
}

// handler: redirect client to URL
void HttpServer::flow_redirect(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpResponse *out = self->out_;

	out->status = http_error::moved_temporarily;
	out->headers.set("Location", argv[1].toString());
	out->finish();

	argv[0] = true;
}

void HttpServer::flow_header_add(void *p, int argc, Flow::Value *argv)
{
}

void HttpServer::flow_header_append(void *p, int argc, Flow::Value *argv)
{
}

void HttpServer::flow_header_overwrite(void *p, int argc, Flow::Value *argv)
{
}

void HttpServer::flow_header_remove(void *p, int argc, Flow::Value *argv)
{
}
// }}}

// {{{ phys
void HttpServer::HttpServer::flow_phys_exists(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = in->fileinfo ? in->fileinfo->exists() : false;
}

void HttpServer::flow_phys_is_dir(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = in->fileinfo ? in->fileinfo->is_directory() : false;
}

void HttpServer::flow_phys_is_reg(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = in->fileinfo ? in->fileinfo->is_regular() : false;
}

void HttpServer::flow_phys_is_exe(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = in->fileinfo ? in->fileinfo->is_executable() : false;
}

void HttpServer::flow_phys_mtime(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = (long long)(in->fileinfo ? in->fileinfo->mtime() : 0);
}

void HttpServer::flow_phys_size(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = (long long)(in->fileinfo ? in->fileinfo->size() : 0);
}

void HttpServer::flow_phys_etag(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = in->fileinfo ? in->fileinfo->etag().c_str() : "";
}

void HttpServer::flow_phys_mimetype(void *p, int argc, Flow::Value *argv)
{
	HttpServer *self = (HttpServer *)p;
	HttpRequest *in = self->in_;

	argv[0] = in->fileinfo ? in->fileinfo->mimetype().c_str() : "";
}
// }}}
// }}}

bool HttpServer::start()
{
	if (!active_)
	{
		active_ = true;

		for (std::list<HttpListener *>::iterator i = listeners_.begin(), e = listeners_.end(); i != e; ++i)
			if (!(*i)->start())
				return false;
	}

	return true;
}

/** tests whether this server has been started or not.
 * \see start(), run()
 */
bool HttpServer::active() const
{
	return active_;
}

/** calls run on the internally referenced io_service.
 * \note use this if you do not have your own main loop.
 * \note automatically starts the server if it wasn't started via \p start() yet.
 */
void HttpServer::run()
{
	if (!active_)
		start();

	while (active_)
	{
		ev_loop(loop_, ev::ONESHOT);
	}
}

void HttpServer::handleRequest(HttpRequest *in, HttpResponse *out)
{
	in_ = in;
	out_ = out;

	if (!onHandleRequest_())
		out->finish();

#if 0
	// pre-request hook
	onPreProcess(const_cast<HttpRequest *>(in));

	// resolve document root
	onResolveDocumentRoot(const_cast<HttpRequest *>(in));

	if (in->document_root.empty())
	{
		out->status = http_error::not_found;
		out->finish();
		return;
	}

	// resolve entity
	in->fileinfo = fileinfo(in->document_root + in->path);
	onResolveEntity(const_cast<HttpRequest *>(in)); // translate_path

	// redirect physical request paths not ending with slash if mapped to directory
	std::string filename = in->fileinfo->filename();
	if (in->fileinfo->is_directory() && !in->path.ends('/'))
	{
		std::stringstream url;

		BufferRef hostname(in->header("X-Forwarded-Host"));
		if (hostname.empty())
			hostname = in->header("Host");

		url << (in->connection.secure ? "https://" : "http://");
		url << hostname.str();
		url << in->path.str();
		url << '/';

		if (!in->query.empty())
			url << '?' << in->query.str();

		//*out *= response_header("Location", url.str());
		out->headers.set("Location", url.str());
		out->status = http_error::moved_permanently;

		out->finish();
		return;
	}

	TRACE(2, "onHandleRequest()...");

	// generate response content, based on this request
	if (!onHandleRequest(in, out))
	{
		out->finish();
	}
#endif
}

HttpListener *HttpServer::listenerByHost(const std::string& hostid) const
{
	int port = extract_port_from_hostid(hostid);

	return listenerByPort(port);
}

std::list<Scope *> HttpServer::getHostsByPort(int port) const
{
	std::list<Scope *> result;

	auto names = hostnames();
	for (auto i = names.begin(), e = names.end(); i != e; ++i)
	{
		if (extract_port_from_hostid(*i) == port)
			result.push_back(resolveHost(*i));
	}

	return result;
}


/**
 * retrieves the listener object that is responsible for the given port number, or null otherwise.
 */
HttpListener *HttpServer::listenerByPort(int port) const
{
	for (auto k = listeners_.begin(); k != listeners_.end(); ++k)
	{
		HttpListener *http_server = *k;

		if (http_server->port() == port)
		{
			return http_server;
		}
	}

	return 0;
}

void HttpServer::pause()
{
	active_ = false;
}

void HttpServer::resume()
{
	active_ = true;
}

void HttpServer::reload()
{
	//! \todo implementation
}

/** unregisters all listeners from the underlying io_service and calls stop on it.
 * \see start(), active(), run()
 */
void HttpServer::stop()
{
	if (active_)
	{
		active_ = false;

		for (std::list<HttpListener *>::iterator k = listeners_.begin(); k != listeners_.end(); ++k)
		{
			(*k)->stop();
		}

		ev_unloop(loop_, ev::ALL);
	}
}

Settings& HttpServer::config()
{
	return settings_;
}

void HttpServer::log(Severity s, const char *msg, ...)
{
	va_list va;
	va_start(va, msg);
	char buf[512];
	int buflen = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	if (colored_log_)
	{
		static AnsiColor::Type colors[] = {
			AnsiColor::Red | AnsiColor::Bold, // error
			AnsiColor::Yellow | AnsiColor::Bold, // warn
			AnsiColor::Green, // info
			static_cast<AnsiColor::Type>(0),
			AnsiColor::Cyan, // debug
		};

		Buffer sb;
		sb.push_back(AnsiColor::make(colors[s + 3]));
		sb.push_back(buf, buflen);
		sb.push_back(AnsiColor::make(AnsiColor::Clear));

		if (logger_)
			logger_->write(s, sb.str());
		else
			std::fprintf(stderr, "%s\n", sb.c_str());
	}
	else
	{
		if (logger_)
			logger_->write(s, buf);
		else
			std::fprintf(stderr, "%s\n", buf);
	}
}

/**
 * sets up a TCP/IP HttpListener on given bind_address and port.
 *
 * If there is already a HttpListener on this bind_address:port pair
 * then no error will be raised.
 */
HttpListener *HttpServer::setupListener(int port, const std::string& bind_address)
{
	// check if we already have an HTTP listener listening on given port
	if (HttpListener *lp = listenerByPort(port))
		return lp;

	// create a new listener
	HttpListener *lp = new HttpListener(*this);

	lp->address(bind_address);
	lp->port(port);

	int value = 0;
	if (!settings_.load("Resources.MaxConnections", value))
		lp->backlog(value);

	listeners_.push_back(lp);

	//log(Severity::debug, "Listening on %s:%d", bind_address.c_str(), port);

	return lp;
}

typedef HttpPlugin *(*plugin_create_t)(HttpServer&, const std::string&);

std::string HttpServer::pluginDirectory() const
{
	return pluginDirectory_;
}

void HttpServer::setPluginDirectory(const std::string& value)
{
	pluginDirectory_ = value;
}

/**
 * loads a plugin into the server.
 *
 * \see plugin, unload_plugin(), loaded_plugins()
 */
HttpPlugin *HttpServer::loadPlugin(const std::string& name, std::error_code& ec)
{
	if (!pluginDirectory_.empty() && pluginDirectory_[pluginDirectory_.size() - 1] != '/')
		pluginDirectory_ += "/";

	std::string filename;
	if (name.find('/') != std::string::npos)
		filename = pluginDirectory_ + name + ".so";
	else
		filename = pluginDirectory_ + name + ".so";

	std::string plugin_create_name("x0plugin_init");

#if !defined(NDEBUG)
	log(Severity::debug, "Loading plugin %s", filename.c_str());
#endif

	Library lib;
	ec = lib.open(filename);
	if (!ec)
	{
		plugin_create_t plugin_create = reinterpret_cast<plugin_create_t>(lib.resolve(plugin_create_name, ec));

		if (!ec)
		{
			HttpPlugin *plugin = plugin_create(*this, name);
			pluginLibraries_[plugin] = std::move(lib);

			return registerPlugin(plugin);
		}
	}

	return NULL;
}

/** safely unloads a plugin. */
void HttpServer::unloadPlugin(const std::string& name)
{
#if !defined(NDEBUG)
	//log(Severity::debug, "Unloading plugin: %s", name.c_str());
#endif

	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
	{
		HttpPlugin *plugin = *i;

		if (plugin->name() == name)
		{
			unregisterPlugin(plugin);

			auto m = pluginLibraries_.find(plugin);
			if (m != pluginLibraries_.end())
			{
				delete plugin;
				m->second.close();
				pluginLibraries_.erase(m);
				return;
			}
		}
	}
}

/** retrieves a list of currently loaded plugins */
std::vector<std::string> HttpServer::pluginsLoaded() const
{
	std::vector<std::string> result;

	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i)
		result.push_back((*i)->name());

	return result;
}

HttpPlugin *HttpServer::registerPlugin(HttpPlugin *plugin)
{
	plugins_.push_back(plugin);

	registerHandler(plugin->name(), &HttpPlugin::process, plugin);

	return plugin;
}

HttpPlugin *HttpServer::unregisterPlugin(HttpPlugin *plugin)
{
	for (auto i = plugins_.begin(), e = plugins_.end(); i != e; ++i) {
		if (*i == plugin) {
			unregisterNative(plugin->name());
			plugins_.erase(i);
			break;
		}
	}

	return plugin;
}

bool HttpServer::declareCVar(const std::string& key, HttpContext cx, const cvar_handler& callback, int priority)
{
	priority = std::min(std::max(priority, -10), 10);

#if 0 // !defined(NDEBUG)
	std::string smask;
	if (cx & HttpContext::server) smask = "server";
	if (cx & HttpContext::host) { if (!smask.empty()) smask += "|"; smask += "host"; }
	if (cx & HttpContext::location) { if (!smask.empty()) smask += "|"; smask += "location"; }

	debug(1, "registering CVAR token=%s, mask=%s, prio=%d", key.c_str(), smask.c_str(), priority);
#endif

	if (cx & HttpContext::server)
		cvars_server_[priority][key] = callback;

	if (cx & HttpContext::host)
		cvars_host_[priority][key] = callback;

	if (cx & HttpContext::location)
		cvars_path_[priority][key] = callback;

	return true;
}

std::vector<std::string> HttpServer::cvars(HttpContext cx) const
{
	std::vector<std::string> result;

	if (cx & HttpContext::server)
		for (auto i = cvars_server_.begin(), e = cvars_server_.end(); i != e; ++i)
			for (auto k = i->second.begin(), m = i->second.end(); k != m; ++k)
				result.push_back(k->first);

	if (cx & HttpContext::host)
		for (auto i = cvars_host_.begin(), e = cvars_host_.end(); i != e; ++i)
			for (auto k = i->second.begin(), m = i->second.end(); k != m; ++k)
				result.push_back(k->first);

	if (cx & HttpContext::location)
		for (auto i = cvars_path_.begin(), e = cvars_path_.end(); i != e; ++i)
			for (auto k = i->second.begin(), m = i->second.end(); k != m; ++k)
				result.push_back(k->first);

	return result;
}

void HttpServer::undeclareCVar(const std::string& key)
{
	//DEBUG("undeclareCVar: '%s'", key.c_str());

	for (auto i = cvars_server_.begin(), e = cvars_server_.end(); i != e; ++i) {
		auto r = i->second.find(key);
		if (r != i->second.end())
			i->second.erase(r);
	}

	for (auto i = cvars_host_.begin(), e = cvars_host_.end(); i != e; ++i) {
		auto r = i->second.find(key);
		if (r != i->second.end())
			i->second.erase(r);
	}

	for (auto i = cvars_path_.begin(), e = cvars_path_.end(); i != e; ++i) {
		auto r = i->second.find(key);
		if (r != i->second.end())
			i->second.erase(r);
	}
}

// {{{ virtual host management
class VirtualHost :
	public ScopeValue
{
public:
	std::string hostid;
	std::vector<std::string> aliases;

	VirtualHost() :
		hostid(),
		aliases()
	{
	}

	virtual void merge(const ScopeValue *)
	{
	}
};

Scope *HttpServer::createHost(const std::string& hostid)
{
	auto i = vhosts_.find(hostid);
	if (i != vhosts_.end())
		return i->second.get(); // XXX trying to create a host that already exists.

	vhosts_[hostid] = std::make_shared<Scope>(hostid);

	VirtualHost *vhost = resolveHost(hostid)->acquire<VirtualHost>(this);
	vhost->hostid = hostid;

	return vhosts_[hostid].get();
}

Scope *HttpServer::createHostAlias(const std::string& master, const std::string& alias)
{
	auto m = vhosts_.find(master);
	if (m == vhosts_.end())
		return NULL; // master hostid not found

	auto a = vhosts_.find(alias);
	if (a != vhosts_.end())
		return NULL; // alias hostid already defined

	resolveHost(master)->acquire<VirtualHost>(this)->aliases.push_back(alias);
	vhosts_[alias] = vhosts_[master];

	return m->second.get();
}

void HttpServer::removeHost(const std::string& hostid)
{
	auto i = vhosts_.find(hostid);
	if (i != vhosts_.end())
		vhosts_.erase(i);
}

void HttpServer::removeHostAlias(const std::string& hostid)
{
	// XXX currently, this is the same.
	removeHost(hostid);
}

std::vector<std::string> HttpServer::hostnames() const
{
	std::vector<std::string> result;

	for (auto i = vhosts_.cbegin(), e = vhosts_.cend(); i != e; ++i)
		if (i->first == i->second->get<VirtualHost>(this)->hostid)
			result.push_back(i->first);

	return result;
}

std::vector<std::string> HttpServer::allHostnames() const
{
	std::vector<std::string> result;

	for (auto i = vhosts_.cbegin(), e = vhosts_.cend(); i != e; ++i)
		result.push_back(i->first);

	return result;
}

std::vector<std::string> HttpServer::hostnamesOf(const std::string& master) const
{
	std::vector<std::string> result;

	const auto i = vhosts_.find(master);
	if (i != vhosts_.end())
	{
		const VirtualHost *vhost = i->second->get<VirtualHost>(this);

		result.push_back(vhost->hostid);
		result.insert(result.end(), vhost->aliases.begin(), vhost->aliases.end());
	}

	return result;
}
// }}}

void HttpServer::addComponent(const std::string& value)
{
	components_.push_back(value);
}

} // namespace x0
