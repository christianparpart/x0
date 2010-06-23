#include <x0/http/HttpCore.h>
#include <x0/http/HttpCore.h>
#include <x0/DateTime.h>
#include <x0/Settings.h>
#include <x0/Scope.h>
#include <x0/Logger.h>

#include <sys/resource.h>
#include <sys/time.h>

namespace x0 {

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

HttpCore::HttpCore(HttpServer& server) :
	HttpPlugin(server, "core"),
	max_fds(std::bind(&HttpCore::getrlimit, this, RLIMIT_CORE),
			std::bind(&HttpCore::setrlimit, this, RLIMIT_NOFILE, std::placeholders::_1))
{
	// register cvars
	declareCVar("Log", HttpContext::server, &HttpCore::setup_logging, -7);
	declareCVar("Resources", HttpContext::server, &HttpCore::setup_resources, -6);
	declareCVar("Plugins", HttpContext::server, &HttpCore::setup_modules, -5);
	declareCVar("ErrorDocuments", HttpContext::server, &HttpCore::setup_error_documents, -4);
	declareCVar("FileInfo", HttpContext::server, &HttpCore::setup_fileinfo, -4);
	declareCVar("Hosts", HttpContext::server, &HttpCore::setup_hosts, -3);
	declareCVar("Advertise", HttpContext::server, &HttpCore::setup_advertise, -2);

	// SSL
#if defined(WITH_SSL)
	declareCVar("SslEnabled", HttpContext::server|HttpContext::host, &HttpCore::setupSslEnabled);
	declareCVar("SslCertFile", HttpContext::server|HttpContext::host, &HttpCore::setupSslCertFile);
	declareCVar("SslKeyFile", HttpContext::server|HttpContext::host, &HttpCore::setupSslKeyFile);
	declareCVar("SslCrlFile", HttpContext::server|HttpContext::host, &HttpCore::setupSslCrlFile);
	declareCVar("SslTrustFile", HttpContext::server|HttpContext::host, &HttpCore::setupSslTrustFile);
#endif
}

HttpCore::~HttpCore()
{
}

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

long long HttpCore::getrlimit(int resource)
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

long long HttpCore::setrlimit(int resource, long long value)
{
	struct rlimit rlim;
	if (::getrlimit(resource, &rlim) == -1)
	{
		server().log(Severity::warn, "Failed to retrieve current resource limit on %s.", rc2str(resource), resource);

		return 0;
	}

	long long last = rlim.rlim_cur;

	// patch against human readable form
	long long hlast = last, hvalue = value;
	switch (resource)
	{
		case RLIMIT_AS:
		case RLIMIT_CORE:
			hlast /= 1024 / 1024;
			value *= 1024 * 1024;
			break;
		default:
			break;
	}

	rlim.rlim_cur = value;
	rlim.rlim_max = value;

	if (::setrlimit(resource, &rlim) == -1) {
		server().log(Severity::warn, "Failed to set resource limit on %s from %lld to %lld.", rc2str(resource), hlast, hvalue);

		return 0;
	}

	debug(1, "Set resource limit on %s from %lld to %lld.", rc2str(resource), hlast, hvalue);

	return value;
}

std::error_code HttpCore::setup_logging(const SettingsValue& cvar, Scope& s)
{
	std::string logmode(cvar["Mode"].as<std::string>());
	auto nowfn = std::bind(&DateTime::htlog_str, &server().now_);

	if (logmode == "file")
		server().logger_.reset(new FileLogger<decltype(nowfn)>(cvar["FileName"].as<std::string>(), nowfn));
	else if (logmode == "null")
		server().logger_.reset(new NullLogger());
	else if (logmode == "stderr")
		server().logger_.reset(new FileLogger<decltype(nowfn)>("/dev/stderr", nowfn));
	else //! \todo add syslog logger
		server().logger_.reset(new NullLogger());

	server().logger_->level(Severity(cvar["Level"].as<std::string>()));

	cvar["Colorize"].load(server().colored_log_);
	return std::error_code();
}

std::error_code HttpCore::setup_modules(const SettingsValue& cvar, Scope& s)
{
	std::error_code ec;

	std::vector<std::string> list;
	cvar["Load"].load(list);

	for (auto i = list.begin(), e = list.end(); i != e; ++i)
		if (!server().loadPlugin(*i, ec))
			return ec;

	return ec;
}

std::error_code HttpCore::setup_resources(const SettingsValue& cvar, Scope& s)
{
	cvar["MaxConnections"].load(server().max_connections);
	cvar["MaxKeepAliveIdle"].load(server().max_keep_alive_idle);
	cvar["MaxReadIdle"].load(server().max_read_idle);
	cvar["MaxWriteIdle"].load(server().max_write_idle);

	cvar["TCP_CORK"].load(server().tcp_cork);
	cvar["TCP_NODELAY"].load(server().tcp_nodelay);

	long long value = 0;
	if (cvar["MaxFiles"].load(value))
		setrlimit(RLIMIT_NOFILE, value);

	if (cvar["MaxAddressSpace"].load(value))
		setrlimit(RLIMIT_AS, value);

	if (cvar["MaxCoreFileSize"].load(value))
		setrlimit(RLIMIT_CORE, value);

	return std::error_code();
}

std::error_code HttpCore::setup_hosts(const SettingsValue& cvar, Scope& s)
{
	std::vector<std::string> hostids = cvar.keys<std::string>();

	for (auto i = hostids.begin(), e = hostids.end(); i != e; ++i)
	{
		std::string hostid = *i;

		server().createHost(hostid);

		auto host_cvars = cvar[hostid].keys<std::string>();

		// handle all vhost-directives
		for (auto pi = server().cvars_host_.begin(), pe = server().cvars_host_.end(); pi != pe; ++pi)
		{
			for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
			{
				if (cvar[hostid].contains(ci->first))
				{
					//debug(1, "CVAR_HOST(%s): %s", hostid.c_str(), ci->first.c_str());
					if (std::error_code ec = ci->second(cvar[hostid][ci->first], server().host(hostid)))
						return ec;
				}
			}
		}

		// handle all path scopes
		for (auto vi = host_cvars.begin(), ve = host_cvars.end(); vi != ve; ++vi)
		{
			std::string path = *vi;
			if (path[0] == '/')
			{
				std::vector<std::string> keys = cvar[hostid][path].keys<std::string>();

				for (auto pi = server().cvars_path_.begin(), pe = server().cvars_path_.end(); pi != pe; ++pi)
					for (auto ci = pi->second.begin(), ce = pi->second.end(); ci != ce; ++ci)
						if (_contains(keys, ci->first))
							;//! \todo ci->second(cvar[hostid][path], vhost(hostid).location(path));

				for (auto ki = keys.begin(), ke = keys.end(); ki != ke; ++ki)
					if (!_contains(server().cvars_path_, *ki))
						server().log(Severity::error, "Unknown location-context variable: '%s'", ki->c_str());
			}
		}
	}

	return std::error_code();
}

std::error_code HttpCore::setup_fileinfo(const SettingsValue& cvar, Scope& s)
{
	std::string value;
	if (cvar["MimeType"]["MimeFile"].load(value))
		server().fileinfo.load_mimetypes(value);

	if (cvar["MimeType"]["DefaultType"].load(value))
		server().fileinfo.default_mimetype(value);

	bool flag = false;
	if (cvar["ETag"]["ConsiderMtime"].load(flag))
		server().fileinfo.etag_consider_mtime(flag);

	if (cvar["ETag"]["ConsiderSize"].load(flag))
		server().fileinfo.etag_consider_size(flag);

	if (cvar["ETag"]["ConsiderInode"].load(flag))
		server().fileinfo.etag_consider_inode(flag);

	return std::error_code();
}

// ErrorDocuments = array of [pair<code, path>]
std::error_code HttpCore::setup_error_documents(const SettingsValue& cvar, Scope& s)
{
	return std::error_code(); //! \todo
}

// Advertise = BOOLEAN
std::error_code HttpCore::setup_advertise(const SettingsValue& cvar, Scope& s)
{
	return cvar.load(server().advertise);
}

// {{{ SSL settings
struct SslSettings :
	public x0::ScopeValue
{
	bool enabled;
	std::string certFileName;
	std::string keyFileName;
	std::string crlFileName;
	std::string trustFileName;

	virtual void merge(const ScopeValue *from)
	{
	}
};

std::error_code HttpCore::setupSslEnabled(const SettingsValue& cvar, Scope& s)
{
	return cvar.load(s.acquire<SslSettings>(this)->enabled);
}

std::error_code HttpCore::setupSslCertFile(const SettingsValue& cvar, Scope& s)
{
	return cvar.load(s.acquire<SslSettings>(this)->certFileName);
}

std::error_code HttpCore::setupSslKeyFile(const SettingsValue& cvar, Scope& s)
{
	return cvar.load(s.acquire<SslSettings>(this)->keyFileName);
}

std::error_code HttpCore::setupSslCrlFile(const SettingsValue& cvar, Scope& s)
{
	return cvar.load(s.acquire<SslSettings>(this)->crlFileName);
}

std::error_code HttpCore::setupSslTrustFile(const SettingsValue& cvar, Scope& s)
{
	return cvar.load(s.acquire<SslSettings>(this)->trustFileName);
}
// }}}

} // namespace x0
